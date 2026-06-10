# Shadow fs.c for the host build. Two transformations:
#   1. Strip `#include <stdio.h>` and `#include <stdarg.h>` — they drag in
#      MSVC libc's FILE typedef which would clash with our struct __FILE.
#      The Keil build needs them (Keil's stdio.h is what forward-declares
#      struct __FILE so fs.h can complete it), so we only strip in the
#      host shadow, not the committed source.
#   2. Stub out the `file_printf` function body. It uses Keil's __va_list
#      and putchar_func_pointer retargeting which aren't on the host.
param(
    [Parameter(Mandatory)] [string]$SrcPath,
    [Parameter(Mandatory)] [string]$Dest
)

$src = Get-Content -Raw -LiteralPath $SrcPath

# (1) strip the system includes
$src = $src -replace '(?m)^#include\s+<stdio\.h>\s*$',  '// <stdio.h>  stripped for host build'
$src = $src -replace '(?m)^#include\s+<stdarg\.h>\s*$', '// <stdarg.h> stripped for host build'

# (2) gut file_printf — Keil-only retargeting.
$src = $src -replace '(?ms)^void file_printf \(FILE \*f, const char \*ptr, \.\.\.\)\s*\{.*?^\}',
@'
void file_printf (FILE *f, const char *ptr, ...) { (void)f; (void)ptr; /* host stub */ }
'@

Set-Content -LiteralPath $Dest -Value $src -Encoding ASCII

if (-not (Test-Path $Dest)) { exit 1 }
exit 0
