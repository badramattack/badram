/**
 * This programs takes alias output file generated by the `fai` tool and checks that the aliases
 * apply to all addresses of the correspondign memory region. The `fai` tool does only check one candidate per memory region
*/

#include<stdbool.h>

#include "mem_range_repo.h"
#include "proc_iomem_parser.h"
#include "helpers.h"
#include "readalias.h"
#include "readalias_ioctls.h"
#include <argp.h>

//cli arguments
struct arguments {
	bool verbose;
	bool acess_reserved;
	char* alias_file_path;
};

typedef struct {
	uint64_t* disfunct_pa;
	size_t disfunct_pa_len;
	size_t access_errors;
	size_t total_pages;
} mr_stats_t;

void free_mr_stats_t(mr_stats_t s) {
	if(s.disfunct_pa != NULL ) {
		free(s.disfunct_pa);
	}
}

/**
 * @brief Checks if `alias` is valid for each page aligned addr in `mr`. Disfunct addresses are
 * returned to the caller
 * @brief mr : memory range to check
 * @brief alias : value to xor to an address in `mr` to obtained the coresponding aliased address
 * @brief out_stats : Outputparam with detailed information about test. Free with `free_mr_stats_t`
 * @return 0 on success. The existence of disfunct addresses is not considered an error
*/
int test_mem_range(mem_range_t mr, uint64_t alias, mr_stats_t* out_stats, struct arguments args) {
	out_stats->disfunct_pa = NULL;
	uint64_t aligned_start = mr.start;
	if( aligned_start & 0xfff ) {
		aligned_start = (aligned_start + 4096) & 0xfff;
	}
	if( aligned_start >= mr.end ) {
		err_log("Weird small memory range: MemRange{.start = 0x%09jx .end=0x%09jx} and aligned_start=0x%09jx\n",
			mr.start, mr.end, aligned_start);
		return -1;
	}

	//sweep over pages, storing pages where alias did not work
	size_t pages_in_mr = (mr.end - aligned_start) /  4096;
	size_t df_next = 0;
	size_t access_errors = 0;
	struct pamemcpy_cfg cfg = {
		.access_reserved = args.acess_reserved,
		.err_on_access_fail = false,
		.flush_method = FM_CLFLUSH,
		.out_stats = {0},
	};
	uint64_t* df = malloc(sizeof(uint64_t) * pages_in_mr);
	for(uint64_t pa = aligned_start; pa < mr.end; pa += 4096 ) {
		uint64_t alias_pa = pa ^ alias;
		//printf("pa 0x%09jx alias_candidate 0x%09jx\n", pa, alias_pa);
		int ret = check_alias(pa, alias_pa, &cfg, false ); 
		if( ret == CHECK_ALIAS_ERR_NO_ALIAS ) {
			df[df_next] = pa;
			df_next += 1;
		} else if( ret == CHECK_ALIAS_ERR_ACCESS ) {
			access_errors += 1;
		}
	}
	df = realloc(df, df_next);
	out_stats->disfunct_pa = df;
	out_stats->disfunct_pa_len = df_next;
	out_stats->access_errors = access_errors;
	out_stats->total_pages = pages_in_mr;

	return 0;
}


/**
 * @brief main function
 * @returns 0 on success
*/
int run(struct arguments args) {

	//parse alias definitions
	mem_range_t* mr = NULL;
	uint64_t* aliases = NULL;
	size_t len;
	if( parse_csv(args.alias_file_path, &mr , &aliases , &len ) ) {
		err_log("Failed to parse aliases from %s\n", args.alias_file_path);
		return -1;
	}
	printf("Parsed %ju mem ranges\n", len);

	if( open_kmod() ) {
		printf("Failed to open kernel module driver. Did you load it?\n");
		goto error;
	}

	//call `test_mem_range` for each mem range and print results
	for(size_t i = 0; i < len; i++ ) {
		mr_stats_t stats;

		double mr_size_gib = (double)(mr[i].end - mr[i].start)/(1<<30);
		printf("[%ju,%ju[ : Checking MemRange{.start=0x%09jx .end=0x%09jx} %0.4f GiB, alias_mask=0x%09jx\n",
			i, len, mr[i].start, mr[i].end, mr_size_gib, aliases[i]);

		if( test_mem_range(mr[i], aliases[i], &stats, args)) {
			err_log("test_mem_range failed\n");
			goto error;
		}
		double access_err_percentage = stats.access_errors / (double)stats.total_pages * 100;
		if( stats.disfunct_pa_len != 0 ) {
			double disfunct_percentage = stats.disfunct_pa_len / (double)stats.total_pages * 100;
			printf("MemRange{.start=0x%09jx .end=0x%09jx} alias 0x%09jx did not work for %0.2f%% of addrs. Access errors for %0.2f%% of addrs\n",
				mr[i].start, mr[i].end, aliases[i], disfunct_percentage, access_err_percentage);
			size_t print_limit = stats.disfunct_pa_len;
			if( !args.verbose && print_limit > 10) {
				printf("Limiting output to 10, start with verbose flag to get all %ju addrs\n", stats.disfunct_pa_len);
				print_limit = 10;
			}
			for(size_t j = 0; j < print_limit; j++ ) {
				printf("\t0x%09jx\n", stats.disfunct_pa[j]);
			}
		} else {
			printf("MemRange{.start=0x%09jx .end=0x%09jx} validated. Access errors on %ju out of %ju accesses (%0.2f%%)\n", mr[i].start, mr[i].end, stats.access_errors, stats.total_pages, access_err_percentage);
		}
		free_mr_stats_t(stats);
	}

	int r = 0;
	goto cleanup;
error:
		r = - 1;
cleanup:
	if( mr ) {
		free(mr);
	}
	if( aliases ) {
		free(aliases);
	}
	return r;
}

const char* argp_program_version = "test_aliases";
const char* argp_program_bug_address = "l.wilke@uni-luebeck.de";
static char doc[] = "Tool to test if the specified alias functions work for addresses in the memory range";
static char args_doc[] = "--aliases [--verbose] [--access-reserved]";
static struct argp_option options[] = {
	{"verbose", 1, 0, 0, "Verbose output", 0},
	{"access-reserved", 2, 0, 0, "Allow accessing reserved memory ranges. Might lead to crashes\n", 0},
	{"aliases", 3, 0, 0, "CSV file (same syntax as fai tool output) that specifies the memory ranges and alias functions\n", 0},
	{0},
};


static error_t parse_opt(int key, char* arg, struct argp_state* state) {
	struct arguments* args = state->input;
	switch(key) {
		case 1:
			args->verbose = true;
			break;
		case 2:
			args->acess_reserved = true;
			break;
		case 3:
			args->alias_file_path = arg;
			break;
		case ARGP_KEY_END:
			if( args->alias_file_path == NULL ) {
				printf("Missing alias file path option\n");
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
	struct arguments args = {
		.acess_reserved = false,
		.alias_file_path = NULL,
		.verbose = false,
	};
	if(argp_parse(&argp, argc, argv, 0, 0, &args)) {
		printf("Failed to parse arguments\n");
		return -1;
	}
	return run(args);
}