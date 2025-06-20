#include "parser.h"
#include "util.h"

#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#define MAX_ERROR 10

struct arg_lit *help, *version;

struct arg_int 
    *max_cpu_time,
    *max_real_time,
    *max_process_number,
    *max_output_size,
    *print_args,
    *uid, *gid;

struct arg_str 
    *max_memory,
    *max_stack,
    *exe_path,
    *input_path,
    *output_path,
    *log_path,
    *exe_args,
    *exe_envs,
    *seccomp_rules;

struct arg_end *end;

void *arg_table[NUM_ALLOWED_ARG + 1];

static void PrintConfig(struct config *_config);

void Initialize(int argc, char **argv, struct config *_config)
{
    arg_table[0] = (help = arg_litn(NULL, "help", 0, 1, "Display help and exit."));
    arg_table[1] = (version = arg_litn(NULL, "version", 0, 1, "Display version info and exit."));
    arg_table[2] = (max_cpu_time = arg_intn(NULL, "max_cpu_time", INT_PLACEHOLDER, 0, 1, "Max cpu running time (ms)."));
    arg_table[3] = (max_real_time = arg_intn(NULL, "max_real_time", INT_PLACEHOLDER, 0, 1, "Max real running time (ms)."));
    arg_table[4] = (max_memory = arg_strn(NULL, "max_memory", STR_PLACEHOLDER, 0, 1, "Max memory (byte)."));
    arg_table[5] = (max_stack = arg_strn(NULL, "max_stack", STR_PLACEHOLDER, 0, 1, "Max stack size (byte, default 16384K)."));
    arg_table[6] = (max_process_number = arg_intn(NULL, "max_process_number", INT_PLACEHOLDER, 0, 1, "Max Process Number"));
    arg_table[7] = (max_output_size = arg_intn(NULL, "max_output_size", INT_PLACEHOLDER, 0, 1, "Max Output Size (byte)"));
    arg_table[8] = (exe_path = arg_str1(NULL, "exe_path", STR_PLACEHOLDER, "Executable file path."));
    arg_table[9] = (input_path = arg_strn(NULL, "input_path", STR_PLACEHOLDER, 0, 1, "Input file path."));
    arg_table[10] = (output_path = arg_strn(NULL, "output_path", STR_PLACEHOLDER, 0, 1, "Output file path."));
    arg_table[11] = (log_path = arg_strn(NULL, "log_path", STR_PLACEHOLDER, 0, 1, "Log file path."));
    arg_table[12] = (exe_args = arg_strn(NULL, "exe_args", STR_PLACEHOLDER, 0, 255, "Arguments for exectuable file."));
    arg_table[13] = (exe_envs = arg_strn(NULL, "exe_envs", STR_PLACEHOLDER, 0, 255, "Environments for executable file."));
    arg_table[14] = (seccomp_rules = arg_strn(NULL, "seccomp_rules", STR_PLACEHOLDER, 0, 1, "Seccomp rules."));
    arg_table[15] = (print_args = arg_intn(NULL, "print_args", INT_PLACEHOLDER, 0, 1, "Print args after config (0 or 1)."));
    arg_table[16] = (uid = arg_intn(NULL, "uid", INT_PLACEHOLDER, 0, 1, "UID for executable file (default `nobody`)."));
    arg_table[17] = (gid = arg_intn(NULL, "gid", INT_PLACEHOLDER, 0, 1, "GID for executable file (default `nobody`)"));
    arg_table[18] = (end = arg_end(MAX_ERROR));

    int nerrors = arg_parse(argc, argv, arg_table);

    if (help->count > 0)
    {
        PrintUsage();
        Halt(0);
    }

    if (version->count > 0)
    {
        PrintVersion();
        Halt(0);
    }

    if (nerrors > 0)
    {
        UnexceptedArg();
        Halt(INVALID_CONFIG);
    }

    signal(SIGINT, Halt);

    InitConfig(_config);

    if (_config->print_args == 1)
        PrintConfig(_config);

    return;
}

void InitConfig(struct config *_config)
{
    int i, nobody_uid, nobody_gid;

    _config->max_cpu_time = max_cpu_time->count > 0 ? (rlim_t)*max_cpu_time->ival : RLIM_INFINITY;
    _config->max_real_time = max_real_time->count > 0 ? (rlim_t)*max_real_time->ival : RLIM_INFINITY;
    _config->max_memory = max_memory->count > 0 ? (rlim_t)atol((char *)max_memory->sval[0]) : RLIM_INFINITY;
    _config->max_stack = max_stack->count > 0 ? (rlim_t)atol((char *)max_stack->sval[0]) : (rlim_t)16 * 1024 * 1024;
    _config->max_process_number = max_process_number->count > 0 ? (rlim_t)*max_process_number->ival : RLIM_INFINITY;
    _config->max_output_size = max_output_size->count > 0 ? (rlim_t)*max_output_size->ival : RLIM_INFINITY;

    if (_config->max_cpu_time == 0) {
        _config->max_cpu_time = RLIM_INFINITY;
    }
    if (_config->max_real_time == 0) {
        _config->max_real_time = RLIM_INFINITY;
    }
    if (_config->max_memory == 0) {
        _config->max_memory = RLIM_INFINITY;
    }
    if (_config->max_stack == 0) {
        _config->max_stack = (rlim_t)16 * 1024 * 1024;
    }
    if (_config->max_process_number == 0) {
        _config->max_process_number = RLIM_INFINITY;
    }
    if (_config->max_output_size == 0) {
        _config->max_output_size = RLIM_INFINITY;
    }

    _config->exe_path = TrimDoubleQuotes((char *)exe_path->sval[0]);
    _config->input_path = input_path->count > 0 ? TrimDoubleQuotes((char *)input_path->sval[0]) : "/dev/stdin";
    _config->output_path = output_path->count > 0 ? TrimDoubleQuotes((char *)output_path->sval[0]) : "/dev/stdout";
    _config->log_path = log_path->count > 0 ? TrimDoubleQuotes((char *)log_path->sval[0]) : "sandbox.log";

    _config->exe_args[0] = _config->exe_path;
    for (i = 1; i <= exe_args->count; i++)
    {
        _config->exe_args[i] = TrimDoubleQuotes((char *)exe_args->sval[i - 1]);
    }
    _config->exe_args[i] = NULL;

    for (i = 0; i < exe_envs->count; i++)
    {
        _config->exe_envs[i] = TrimDoubleQuotes((char *)exe_envs->sval[i]);
    }
    if (exe_envs->count == 0)
    {
        extern char **environ;
        for (i = 0; environ[i] != NULL && i < MAX_ENV; i++)
        {
            _config->exe_envs[i] = environ[i];
        }
    }
    _config->exe_envs[i] = NULL;

    _config->seccomp_rules = seccomp_rules->count > 0 ? TrimDoubleQuotes((char *)seccomp_rules->sval[0]) : NULL;

    GetNobody(&nobody_uid, &nobody_gid);
    _config->uid = uid->count > 0 ? (uid_t)*uid->ival : nobody_uid;
    _config->gid = gid->count > 0 ? (gid_t)*gid->ival : nobody_gid;
    _config->print_args = print_args->count > 0 ? *print_args->ival : 0;
}

static void PrintConfig(struct config *_config)
{
    int i;
    printf("max_cpu_time: %llu\n", (unsigned long long)_config->max_cpu_time);
    printf("max_real_time: %llu\n", (unsigned long long)_config->max_real_time);
    printf("max_memory: %llu\n", (unsigned long long)_config->max_memory);
    printf("max_stack: %llu\n", (unsigned long long)_config->max_stack);
    printf("max_process_number: %llu\n", (unsigned long long)_config->max_process_number);
    printf("max_output_size: %llu\n", (unsigned long long)_config->max_output_size);
    printf("exe_path: %s\n", _config->exe_path ? _config->exe_path : "(null)");
    printf("input_path: %s\n", _config->input_path ? _config->input_path : "(null)");
    printf("output_path: %s\n", _config->output_path ? _config->output_path : "(null)");
    printf("log_path: %s\n", _config->log_path ? _config->log_path : "(null)");
    for (i = 0; _config->exe_args[i] != NULL; i++)
    {
        printf("exe_args[%d]: %s\n", i, _config->exe_args[i]);
    }
    for (i = 0; _config->exe_envs[i] != NULL; i++)
    {
        printf("exe_envs[%d]: %s\n", i, _config->exe_envs[i]);
    }
    printf("seccomp_rules: %s\n", _config->seccomp_rules ? _config->seccomp_rules : "(null)");
    printf("uid: %d\n", _config->uid);
    printf("gid: %d\n", _config->gid);
    printf("print_args: %d\n", _config->print_args);
}
