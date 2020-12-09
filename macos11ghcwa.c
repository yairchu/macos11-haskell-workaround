#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// From https://opensource.apple.com/source/dyld/dyld-97.1/include/mach-o/dyld-interposing.h.auto.html
#define DYLD_INTERPOSE(_replacment,_replacee) \
    __attribute__((used)) \
    static struct{ const void* replacment; const void* replacee; } _interpose_##_replacee \
        __attribute__ ((section ("__DATA,__interpose"))) = \
        { (const void*)(unsigned long)&_replacment, (const void*)(unsigned long)&_replacee };

int my_stat (const char* restrict path, struct stat* restrict buf)
{
    const char prefix[] = "/System/Library/Frameworks/";
    if (0 == strncmp (path, prefix, sizeof (prefix) - 1))
    {
        // Make believe that system framework files exists to work-around GHC bug
        return 0;
    }
    return stat (path, buf);
}

DYLD_INTERPOSE (my_stat, stat)

// From https://stackoverflow.com/a/37188697/40916
static int string_ends_with (const char *str, const char *suffix)
{
    const int str_len = strlen (str), suffix_len = strlen (suffix);
    return str_len >= suffix_len && 0 == strcmp (str + (str_len - suffix_len), suffix);
}

static char* append (const char* x, const char* y)
{
    const int len_x = strlen (x), len_y = strlen (y);
    char* result = malloc (len_x + len_y + 1);
    memcpy (result, x, len_x);
    memcpy (result + len_x, y, len_y);
    result[len_x + len_y] = '\0';
    return result;
}

static char* remove_suffix (const char* s, const char* suffix)
{
    if (! string_ends_with (s, suffix))
        return NULL;
    char* prefix = strdup (s);
    prefix[strlen(s) - strlen(suffix) + 1] = '\0';
    return prefix;
}

static int num_elems (char* const elems[])
{
    int result = 0;
    for (; *elems; ++elems)
        ++result;
    return result;
}

static char** fix_env (const char* folder, const char* topdir, char* const envp[])
{
    const int env_size = num_elems (envp);
    char** new_env = malloc ((env_size + 7) * sizeof (char* const));
    memcpy (new_env, envp, env_size * sizeof (char*));
    const char* exedir = append (topdir, "/bin");
    new_env[env_size] = append ("exedir=", exedir);
    new_env[env_size+1] = "exeprog=ghc-stage2";
    new_env[env_size+2] = append ("datadir=", append (folder, "share"));
    new_env[env_size+3] = append ("bindir=", append (folder, "bin"));
    new_env[env_size+4] = append ("topdir=", topdir);
    new_env[env_size+5] = append ("executablename=", append (exedir, "/ghc"));
    new_env[env_size+6] = NULL;

    return new_env;
}

static char** fix_argv (const char* topdir, char* const argv[])
{
    const int argc = num_elems (argv);
    char** new_argv = malloc ((argc + 2) * sizeof (char* const));
    new_argv[0] = argv[0];
    memcpy (new_argv + 2, argv + 1, (argc + 1) * sizeof (char*));
    new_argv[1] = append ("-B", topdir);
    return new_argv;
}

static char* get_ghc_ver (const char* folder)
{
    const int len = strlen (folder);
    int i = len - 2;
    while (i > 1 && folder[i-1] != '/')
        --i;
    char* result = strdup (folder + i);
    result[len - i - 1] = '\0';
    return result;    
}

int my_execve (const char* file, char* const argv[], char* const envp[])
{
    const char* folder = remove_suffix (file, "/bin/ghc");
    if (folder == NULL)
        return execve (file, argv, envp);

    // Execution of ghc script.
    // Because macOS removes the DYLD injection for scripts,
    // we emulate its behaviour ourselves.
    const char* ghc_ver = get_ghc_ver (folder);
    const char* executable = append (folder, append ("lib/", append (ghc_ver, "/bin/ghc")));
    const char* topdir = append (folder, append ("lib/", ghc_ver));
    return execve (executable, fix_argv (topdir, argv), fix_env (folder, topdir, envp));
}

DYLD_INTERPOSE (my_execve, execve)
