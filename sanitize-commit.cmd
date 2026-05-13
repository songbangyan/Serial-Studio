@echo off
rem
rem sanitize-commit.cmd -- Windows companion to sanitize-commit.sh
rem
rem Mirrors the bash script's pipeline:
rem  - expand-doxygen.py        -> one-liner /** ... */ to canonical 3-line form
rem  - clang-format pass 1      -> normalize layout
rem  - code-verify.py --fix     -> rules clang-format can't express
rem  - clang-format pass 2      -> reflow after code-verify's edits
rem  - code-verify.py --check   -> regenerate .code-report
rem  - documentation-verify.py  -> Markdown AI-narration scan
rem  - build_search_index.py    -> refresh AI assistant BM25 index
rem  - prompt to commit (Conventional Commits) and optionally push
rem
rem Windows does not have POSIX file permissions, so the chmod stage from
rem the bash script is intentionally skipped.
rem
rem Usage:  sanitize-commit.cmd
rem
rem License: GNU General Public License v3.0
rem https://www.gnu.org/licenses/gpl-3.0.html
rem
rem Author: Alex Spataru <https://github.com/alex-spataru>

setlocal enabledelayedexpansion

rem Go to the repo root
for /f "usebackq tokens=*" %%i in (`git rev-parse --show-toplevel 2^>nul`) do set "REPO_ROOT=%%i"
if not defined REPO_ROOT (
    echo Error: not inside a git repository.
    exit /b 1
)
cd /d "%REPO_ROOT%" || exit /b 1

rem Resolve a Python interpreter -- prefer "python", fall back to "py -3"
set "PY=python"
where %PY% >nul 2>&1
if errorlevel 1 (
    set "PY=py -3"
    where py >nul 2>&1
    if errorlevel 1 (
        echo Error: neither "python" nor "py" found on PATH.
        exit /b 1
    )
)

rem expand-doxygen
if exist scripts\expand-doxygen.py (
    echo Expanding single-line doxygen comments...
    %PY% scripts\expand-doxygen.py
    if errorlevel 1 echo expand-doxygen failed
)

echo Running clang-format (pass 1)...
call :run_clang_format

if exist scripts\code-verify.py (
    echo Running code-verify...
    %PY% scripts\code-verify.py --fix
    if errorlevel 1 echo code-verify failed
)

echo Running clang-format (pass 2)...
call :run_clang_format

if exist scripts\code-verify.py (
    echo Regenerating .code-report...
    %PY% scripts\code-verify.py --check >nul
    if errorlevel 1 echo code-verify --check found issues
)

if exist scripts\documentation-verify.py (
    echo Running documentation-verify...
    %PY% scripts\documentation-verify.py --quiet
    if errorlevel 1 echo documentation-verify found issues
)

if exist app\rcc\ai\build_search_index.py (
    echo Rebuilding AI search index...
    %PY% app\rcc\ai\build_search_index.py
    if errorlevel 1 echo build_search_index failed
)

echo Checking for changes...

rem Detect any change (staged or unstaged). `git status --porcelain` prints
rem one line per file; if there's no output, the working tree is clean.
set "DIRTY="
for /f "usebackq delims=" %%L in (`git status --porcelain`) do set "DIRTY=1"
if not defined DIRTY (
    echo No changes detected.
    exit /b 0
)

echo.
echo Changed files:
git status --short
echo.

rem Count files in the eventual commit (staged if any, otherwise unstaged)
set /a COUNT=0
for /f "usebackq delims=" %%L in (`git diff --cached --name-only`) do set /a COUNT+=1
if !COUNT! equ 0 (
    for /f "usebackq delims=" %%L in (`git diff --name-only`) do set /a COUNT+=1
)
echo !COUNT! file(s) changed.

set "ANSWER="
set /p "ANSWER=Do you want to commit and push these changes? [y/N] "
if /i not "!ANSWER!"=="y" (
    echo Aborting.
    exit /b 0
)

:get_message
echo.
echo Enter a Conventional Commit message (e.g., 'fix: correct permission issue'):
set "COMMIT_MSG="
set /p "COMMIT_MSG=> "
if not defined COMMIT_MSG (
    echo Commit message cannot be empty.
    goto get_message
)

rem Round-trip the message through a temp file so we can validate it and pass
rem it to `git commit -F` -- avoids every CMD escaping pitfall for `, &, <, >, |, ".
set "MSG_FILE=%TEMP%\ss_commit_msg_%RANDOM%%RANDOM%.txt"
>"%MSG_FILE%" echo !COMMIT_MSG!

%PY% -c "import sys, re, pathlib; msg = pathlib.Path(sys.argv[1]).read_text(encoding='utf-8').strip(); sys.exit(0 if re.match(r'^(feat|fix|chore|docs|style|refactor|perf|test)(\(.+\))?: .+', msg) else 1)" "%MSG_FILE%"
if errorlevel 1 (
    del /q "%MSG_FILE%" >nul 2>&1
    echo Invalid commit message format. Use Conventional Commits ^(e.g., 'feat: add new thing'^).
    goto get_message
)

git add .
if errorlevel 1 (
    del /q "%MSG_FILE%" >nul 2>&1
    echo git add failed.
    exit /b 1
)

git commit -F "%MSG_FILE%"
set "COMMIT_RC=!ERRORLEVEL!"
del /q "%MSG_FILE%" >nul 2>&1
if not "!COMMIT_RC!"=="0" (
    echo git commit failed.
    exit /b !COMMIT_RC!
)

for /f "usebackq tokens=*" %%i in (`git rev-parse --abbrev-ref HEAD`) do set "BRANCH=%%i"

set "PUSH_CONFIRM="
set /p "PUSH_CONFIRM=Push to origin/!BRANCH!? [y/N] "
if /i "!PUSH_CONFIRM!"=="y" (
    git push origin "!BRANCH!"
    if errorlevel 1 (
        echo git push failed.
        exit /b 1
    )
    echo Changes pushed.
) else (
    echo Changes committed but not pushed.
)

endlocal
exit /b 0

rem ---------------------------------------------------------------------------
rem clang-format helper
rem ---------------------------------------------------------------------------
:run_clang_format
where clang-format >nul 2>&1
if errorlevel 1 (
    echo clang-format not on PATH -- skipping.
    exit /b 0
)
for %%D in (app doc examples) do (
    if exist "%%D\" (
        for /r "%%D" %%F in (*.cpp *.h *.c) do (
            if /i not "%%~nxF"=="miniaudio.h" (
                clang-format -i "%%F" >nul 2>&1
                if errorlevel 1 echo clang-format failed on "%%F"
            )
        )
    )
)
exit /b 0
