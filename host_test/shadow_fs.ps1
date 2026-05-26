# Shadow fs.c for the host build. Three transformations:
#   1. Strip `#include <stdio.h>` and `#include <stdarg.h>` (drag in MSVC libc).
#   2. Inject the global declarations that the 2022 incomplete refactor left
#      commented out (the bare names `open_file_list`, `current_fat`, etc.
#      are referenced throughout fs.c but never defined in the committed file).
#   3. Stub out the `file_printf` function body (Keil-only __va_list).
param(
    [Parameter(Mandatory)] [string]$SrcPath,
    [Parameter(Mandatory)] [string]$Dest
)

$src = Get-Content -Raw -LiteralPath $SrcPath

# (1) strip the system includes
$src = $src -replace '(?m)^#include\s+<stdio\.h>\s*$',  '// <stdio.h>  stripped for host build'
$src = $src -replace '(?m)^#include\s+<stdarg\.h>\s*$', '// <stdarg.h> stripped for host build'

# (2) inject the missing global declarations. We place them just after the
# existing `FILE *putchar_f = NULL;` line so they're in the same scope as the
# original (pre-refactor) code expected.
$globals = @'

// === HOST-BUILD INJECTION ===
// The 2022 refactor moved these globals into an fs_t singleton but the bodies
// of fs.c still reference the bare names. Wire them up here so the host build
// links. This injection is build-time only; the committed source is unchanged.
list_t          *open_file_list           = NULL;
FAT             current_fat               = { {0,0,0,0}, 0, 0, 0 };
uint32_t        next_file_num             = 1;
int             open_file_list_size       = 0;
uint32_t        start_search_page         = 0;
uint32_t        wear_threshold            = WEAR_THRESHOLD;
// === END HOST-BUILD INJECTION ===
'@

$src = $src -replace '(?m)^(FILE\s+\*putchar_f\s*=\s*NULL\s*;)', "`$1`r`n$globals"

# (3) gut file_printf — it uses Keil's __va_list and putchar_func_pointer hooks.
# Replace the function body with an empty stub so it links if anyone calls it.
$src = $src -replace '(?ms)^void file_printf \(FILE \*f, const char \*ptr, \.\.\.\)\s*\{.*?^\}',
@'
void file_printf (FILE *f, const char *ptr, ...) { (void)f; (void)ptr; /* host stub */ }
'@

# (4) Patch the half-finished refactor: fs_fcloseall is declared as
# `void fs_fcloseall(fs_t *fs)` but is called bare as `fs_fcloseall()` at one
# site. Add the missing NULL arg so the call links.
$src = $src -replace 'fs_fcloseall\s*\(\s*\)', 'fs_fcloseall(NULL)'

Set-Content -LiteralPath $Dest -Value $src -Encoding ASCII

if (-not (Test-Path $Dest)) { exit 1 }
exit 0
