# Repository Guidance

Please read projects/TESTING.md to understand our methodology for
doing tests.

Please also read CODE_STANDARDS.md before making source changes. It
captures repository coding, documentation, testing, and review
expectations that supplement this guidance.

Our dogma is that tests must provide us with complete assurance that
changes have not broken the functionality of the system. We seek 100%
test coverage with fully automated tests. We accept that the tests
may, in the end, be much, much longer than the code itself. We always
add tests when we are making changes that cover the changes so we are
sure they worked.

We must always design code for testability. If we discover that code
is not testable, we must rearchitect it to be testable. Sometimes we
will discover that code should be rewritten to make it cleaner and
better, and to make tests easier a the same time; always point out
such opportunities.

Assume C17 on all host platforms and favor removing support for older
versions of C and the C standard. Our two supported host platform
types are POSIX and recent Windows; assume a recent POSIX on all POSIX
platforms and recent levels of Windows APIs.

When adding new source code to this repository, prefer keeping lines
under 80 columns.

When adding or touching include blocks in C source files, keep system
includes alphabetized where practical. Do this in a way that preserves
required include ordering and keeps conditional platform blocks readable;
do not force alphabetical order when it would obscure intent or risk
changing behavior.

When doing source code commits, create a commit message in tmp/ in the
repository and make sure it is propery formatted, including keeping
lines under 80 columns, and then use that with git's `-F` flag. Give
it a temporary unique name to avoid collisions with other commit
workflows.

When creating a new source file, run `clang-format` on that new file
before finishing the change. Do not run `clang-format` across existing
legacy files just to normalize formatting.

When modifying old source files, try not to mess up the location
column of trailing comments on the same line when there is a series of
lined up comments. When adding brand new functions to old source
files, it is preferred to use sane code formatting and indentation
even if the old file has weird formatting.

When adding new functions or other new code to an existing file, write
the new code so that it roughly follows the style in `.clang-format`
without reformatting unrelated older code around it. The goal is for new
code to move toward the project style while avoiding large formatting
churn in legacy files.

Before starting a long block of work, check that the tools needed for
that work are present in the environment.

If a required tool is missing, stop and ask for it to be installed
instead of trying to work around the missing dependency.

Names are important, they are a form of documentation. Think carefully
about good names for functions, variables, constants, etc. Don't make
names too long without a good reason, that makes lines too wide and
thus difficult to read!

If you discover an ugly design issue or major or even minor
architectural problem that requires repair, do not keep it to
yourself. Inform the user, and create a project file in projects/ to
document the needed refactorings to fix later.

Don't pathologically minimize the scope of changes if doing so creates
ugly problems later.

If moving code from one part of a file to another makes things
cleaner, then do that.

Comment what all non-obvious functions do.

Do not try to run multiple git commands in parallel; they will race
each other and conflict. Similarly, do not try to run multiple cmake
and ctest commands in parallel, they will race each other and
conflict.

Use `cmake --build` with Ninja and no -j to make sure we get optimal
parallelism.

Use `ctest --parallel` to make sure tests are run in parallel.

On macOS, prefer the Clang/LLVM toolchain consistently:

- use `clang` rather than GCC
- use `llvm-cov` and `llvm-profdata` for code coverage
- use `lldb` rather than `gdb`

Do not switch to GCC or GNU coverage tools on macOS unless the user
explicitly asks for that.
