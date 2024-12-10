#include <asm/io.h>         // ioremap, iounmap
#include <linux/cdev.h>     // device_create, ...
#include <linux/highmem.h>  // kmap, kunmap
#include <linux/module.h>
#include <linux/version.h>
#include <linux/io.h>

#include "include/readalias_ioctls.h"

typedef unsigned long long ull;

enum mapping_type {
  MT_VMAP,
  MT_IOREMAP,
  MT_MEMREMAP,
};



/**
 * @brief Try to map pfn using various strategies
 * @paramter pfn : page we want to map
 * @parameter out_mt : Output paramter, filled with the mapping type that succeeded
 * @paramter out_page : Output parameter, might be filled with reference to page struct, depending on the mapping type
 * @returns valid mapping or NULL on error
*/
void* custom_map(uint64_t pfn, enum mapping_type* out_mt, struct page** out_page) {
  if(pfn_valid(pfn)) {
    *out_mt = MT_VMAP;
    return vmap(out_page, 1, 0, PAGE_KERNEL_IO_NOCACHE);
  }
  void* mapping = ioremap(pfn << PAGE_SHIFT, PAGE_SIZE);
  if(mapping) {
    *out_mt = MT_IOREMAP;
    return mapping;
  }
  *out_mt = MT_MEMREMAP;
  return memremap(pfn << PAGE_SHIFT, PAGE_SIZE, MEMREMAP_WB | MEMREMAP_WT | MEMREMAP_WC | MEMREMAP_ENC | MEMREMAP_DEC);
}

/**
 * @brief : Unmap memory allocated with custom_map
 * @parameter mapping : mapping that should me unmapped
 * @parameter mt : mapping_type returned by custom_map
*/
void custom_unmap(void* mapping, enum mapping_type mt) {
  switch(mt) {
  case MT_VMAP:
    vfree(mapping);
    break;
  case MT_IOREMAP:
    iounmap((void __iomem*)mapping);
    break;
  case MT_MEMREMAP:
    memunmap(mapping);
    break;
  default:
    printk("%s:%d unsupported mapping type %d\n", __FILE__, __LINE__, (int)mt);
    break;
  }
}

#define NAME "readalias"
#define CACHELINE_SIZE 64


static int major = -1;
static struct cdev mycdev;
static struct class *myclass = NULL;

unsigned char buffer[PAGE_SIZE];


void clflush_range(void* p, size_t size) {
  size_t i;
  for (i = 0; i < size; i += CACHELINE_SIZE) {
    asm volatile("clflush 0(%0)\n" : : "c"(p+i) : "rax");
  }
  asm volatile("mfence\n");
}


void wbinvd_ac(void) {
  wbinvd_on_all_cpus();
}

void custom_flush(void* mem, size_t bytes, enum flush_method fm) {
  switch(fm) {
  case FM_NONE: //No action required
    break;
  case FM_CLFLUSH:
    return clflush_range(mem, bytes);
    break;
  case FM_WBINVD:
    return wbinvd_ac();
    break;
  default:
    printk("%s:%d invalid flush method %d\n", __FILE__, __LINE__, (int)fm);
    break;    
  }
}

/**
 * Copy memory to destination.
 * 
 * @param pa: The physical address to the page to flush. Any physical address
 *            within the target page works.
 * @param access_reserved : if true, try to access pa even if it is marked as reserved
 * 
 * @returns Whether the flushing was successful.
 */
int flush_page(ull pa, int access_reserved ) {
  ull pfn;
  struct page* page;
  enum mapping_type mapping_type;
  unsigned char* current_page;

  //printk("trying to flush 0x%llx\n", pa);
  // Temporarily disable SMAP, which prevents the kernel from reading/writing user-space memory
  __uaccess_begin_nospec();

  pfn = pa >> PAGE_SHIFT;
  page = pfn_to_page(pfn);

  if (pfn_valid(pfn) && PageReserved(page) && !access_reserved) {
    // printk("pfn valid but reserved page\n");
    return RET_RESERVED;
  }

  current_page = custom_map(pfn, &mapping_type, &page);
  if (!current_page) {
    printk("flush_page: failed to map 0x%llx\n", pa);
    return RET_MAPFAIL;
  }

  clflush_range(current_page, PAGE_SIZE);

  custom_unmap(current_page, mapping_type);

  // Re-enable SMAP
  __uaccess_end();
  //printk("flushh successfull\n");
  return 0;
}

/**
 * Copy memory to destination.
 * 
 * @param dst: The physical address of the destination. All destination bytes
 *             must lay within the same page.
 * @param src: The buffer to copy from.
 * @param count: The number of bytes to copy.
 * @param flush: Whether to flush the page after writing to it and type of flush. This ensures the
 *               data is written to DRAM.
 * 
 * @returns Whether the copy was successful.
 */
int memcpy_topage(ull dst, const void* src, size_t count, enum flush_method flush_method, int access_reserved) {
  ull pfn, offset;
  struct page* page;
  enum mapping_type mapping_type;
  unsigned char* current_page;

  // Temporarily disable SMAP, which prevents the kernel from reading/writing user-space memory
  __uaccess_begin_nospec();

  pfn = dst >> PAGE_SHIFT;
  page = pfn_to_page(pfn);

  // Writing to reserved pages can cause the system to crash
  if (pfn_valid(pfn) && PageReserved(page) && !access_reserved) {
    return RET_RESERVED;
  }

  current_page = custom_map(pfn, &mapping_type, &page);
  if (!current_page) {
    printk("memcpy_topage: failed to map 0x%llx\n", dst);
    return RET_MAPFAIL;
  }

  offset = dst % PAGE_SIZE;
  if (offset + count > PAGE_SIZE) return -1;
  memcpy(current_page + offset, src, count);

  // Flushing ensures the data is written to DRAM
  custom_flush(current_page, PAGE_SIZE, flush_method);

  custom_unmap(current_page, mapping_type);
  // Re-enable SMAP
  __uaccess_end();

  return 0;
}

/**
 * Copy memory to buffer.
 * 
 * @param dst: The buffer to copy to.
 * @param src: The physical address to copy from. All source bytes must lay
 *             within the same page.
 * @param count: The number of bytes to copy.
 * @param flush: Whether to flush the page before reading from it. This ensures
 *               the data is read from DRAM.
 * 
 * @returns Whether the copy was successful.
 */
int memcpy_frompage(void* dst, ull src, size_t count, enum flush_method flush_method, int access_reserved) {
  ull pfn, offset;
  struct page* page;
  enum mapping_type mapping_type;
  unsigned char* current_page;


  // Temporarily disable SMAP, which prevents the kernel from reading/writing user-space memory
  __uaccess_begin_nospec();

  pfn = src >> PAGE_SHIFT;
  page = pfn_to_page(pfn);

  if (pfn_valid(pfn) && PageReserved(page) && !access_reserved) {
  //   printk("memcpy_frompage: src=0x%llx : reserved page (pfn_valid=%d)\n", src, pfn_valid(pfn));
    return RET_RESERVED;
  }

  current_page = custom_map(pfn, &mapping_type, &page);
  if (!current_page) {
    printk("memcpy_frompage: src=0x%llx : failed to map\n", src);
    return RET_MAPFAIL;
  }

  offset = src % PAGE_SIZE;
  if (offset + count > PAGE_SIZE) {
    printk("memcpy_frompage: src=0x%llx : offset + count larger than page size\n", src);
    return -1;
  }

  custom_flush(current_page, PAGE_SIZE, flush_method);

  memcpy(dst, current_page + offset, count);

  custom_unmap(current_page, mapping_type);
  
  // Re-enable SMAP
  __uaccess_end();

  return 0;
}

static int open(struct inode *inode, struct file *file)
{
  (void)inode; (void)file;
  printk("Opened module.\n");
  return 0;
}

static int close(struct inode *inode, struct file *file)
{
  (void)inode; (void)file;
  printk("Closed module.\n");
  return 0;
}

static long ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
  struct args args;
  (void)file;
  
  switch(cmd) {
    case MEMCPY_TOPA:
      if (copy_from_user(&args, (const void*)arg, sizeof(args))) return -1;
      if (copy_from_user(buffer, args.buffer, args.count)) return -1;
      return memcpy_topage(args.pa, buffer, args.count, args.flush, args.access_reserved);
    case MEMCPY_FROMPA: {
      int cpy_ret;
       if (copy_from_user(&args, (const void*)arg, sizeof(args))) return -1;
      //we use the retval to communicate reserved page and mapping errs separately
      // only < 0 is a hard error. store retval to return it after copy to user
      cpy_ret = memcpy_frompage(buffer, args.pa, args.count, args.flush, args.access_reserved);
      if ( cpy_ret < 0) {
        return cpy_ret;
      }
      if( copy_to_user(args.buffer, buffer, args.count) ) {
        return -1;
      }
      return cpy_ret;
    }
     
    case FLUSH_PAGE:
      if (copy_from_user(&args, (const void*)arg, sizeof(args))) return -1;
      return flush_page(args.pa, args.access_reserved);
    case WBINVD_AC:
      wbinvd_ac();
      return 0;
    default:
      printk("Unknown cmd=%ud\n", cmd);
      break;
  }
  return 0;
}

static const struct file_operations fops = {
  .open = open,
  .owner = THIS_MODULE,
  .release = close,
  .unlocked_ioctl = ioctl,
};

static void cleanup(int device_created)
{
  if (device_created) {
    device_destroy(myclass, major);
    cdev_del(&mycdev);
  }
  if (myclass) class_destroy(myclass);
  if (major != -1) unregister_chrdev_region(major, 1);
}

static int __init findpattern_init(void) {
  int device_created = 0;

  /* /proc/devices */
  if (alloc_chrdev_region(&major, 0, 1, NAME "_proc") < 0)
    goto error;
  /* /sys/class */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
  if ((myclass = class_create("readalias_sys")) == NULL)
    goto error;
#else
  if ((myclass = class_create(THIS_MODULE, NAME "_sys")) == NULL)
    goto error;
#endif
  /* /dev/ */
  if (device_create(myclass, NULL, major, NULL, NAME "_dev") == NULL)
    goto error;
  device_created = 1;
  cdev_init(&mycdev, &fops);
  if (cdev_add(&mycdev, major, 1) == -1)
    goto error;

  printk("Inserted module.\n");

  return 0;
error:
    cleanup(device_created);
    return -1;
}

static void __exit findpattern_exit(void) {
  cleanup(1);
	printk("Removed module.\n");
}

module_init(findpattern_init);
module_exit(findpattern_exit);

MODULE_LICENSE("GPL");
