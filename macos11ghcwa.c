#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

// From https://opensource.apple.com/source/dyld/dyld-97.1/include/mach-o/dyld-interposing.h.auto.html
#define DYLD_INTERPOSE(_replacment, _replacee)                                    \
    __attribute__ ((used)) static struct                                          \
    {                                                                             \
        const void* replacment;                                                   \
        const void* replacee;                                                     \
    } _interpose_##_replacee __attribute__ ((section ("__DATA,__interpose"))) = { \
        (const void*) (unsigned long) &_replacment, (const void*) (unsigned long) &_replacee};

#define STARTS_WITH(prefix, str) (0 == strncmp (str, prefix, sizeof (prefix) - 1))

int my_stat (const char* restrict path, struct stat* restrict buf)
{
    if (STARTS_WITH ("/System/Library/Frameworks/", path))
    {
        // Make believe that system framework files exists to work-around GHC bug
        return 0;
    }
    return stat (path, buf);
}

DYLD_INTERPOSE (my_stat, stat)

// Intercept ghc and runghc scripts.
// macOS removes the DYLD injection for scripts,
// so we emulate their behaviour ourselves instead of executing them.

// From https://stackoverflow.com/a/37188697/40916
static int string_ends_with (const char* str, const char* suffix)
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
    prefix[strlen (s) - strlen (suffix) + 1] = '\0';
    return prefix;
}

static int num_elems (char* const elems[])
{
    int result = 0;
    for (; *elems; ++elems)
        ++result;
    return result;
}

static char** fix_env (const char* folder, const char* exeprog, const char* topdir, char* const envp[])
{
    const int env_size = num_elems (envp);
    char** new_env = malloc ((env_size + 7) * sizeof (char* const));
    memcpy (new_env, envp, env_size * sizeof (char*));
    const char* exedir = append (topdir, "/bin");
    new_env[env_size] = append ("exedir=", exedir);
    new_env[env_size + 1] = append ("exeprog=", exeprog);
    new_env[env_size + 2] = append ("datadir=", append (folder, "share"));
    new_env[env_size + 3] = append ("bindir=", append (folder, "bin"));
    new_env[env_size + 4] = append ("topdir=", topdir);
    new_env[env_size + 5] = append ("executablename=", append (exedir, "/ghc"));
    new_env[env_size + 6] = NULL;
    return new_env;
}

static char** fix_ghc_argv (const char* topdir, char* const argv[])
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
    while (i > 1 && folder[i - 1] != '/')
        --i;
    char* result = strdup (folder + i);
    result[len - i - 1] = '\0';
    return result;
}

static int exec_ghc (const char* folder, char* const argv[], char* const envp[])
{
    const char* ghc_ver = get_ghc_ver (folder);
    const char* executable = append (folder, append ("lib/", append (ghc_ver, "/bin/ghc")));
    const char* topdir = append (folder, append ("lib/", ghc_ver));
    return execve (executable, fix_ghc_argv (topdir, argv), fix_env (folder, "ghc-stage2", topdir, envp));
}

static char** fix_runghc_argv (const char* folder, char* const argv[])
{
    const int argc = num_elems (argv);
    char** new_argv = malloc ((argc + 3) * sizeof (char* const));
    new_argv[0] = argv[0];
    memcpy (new_argv + 3, argv + 1, (argc + 1) * sizeof (char*));
    new_argv[1] = "-f";
    new_argv[2] = append (folder, "bin/ghc");
    return new_argv;
}

static int exec_runghc (const char* folder, char* const argv[], char* const envp[])
{
    const char* ghc_ver = get_ghc_ver (folder);
    const char* executable = append (folder, append ("lib/", append (ghc_ver, "/bin/runghc")));
    return execve (
        executable, fix_runghc_argv (folder, argv),
        fix_env (folder, "runghc", append (folder, append ("lib/", ghc_ver)), envp));
}

int my_execve (const char* file, char* const argv[], char* const envp[])
{
    const char* folder = remove_suffix (file, "/bin/ghc");
    if (folder != NULL)
        return exec_ghc (folder, argv, envp);
    folder = remove_suffix (file, "/bin/runghc");
    if (folder != NULL)
        return exec_runghc (folder, argv, envp);
    return execve (file, argv, envp);
}

DYLD_INTERPOSE (my_execve, execve)
