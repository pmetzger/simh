# ZIMH pre-built binaries

Welcome to ZIMH's pre-built binaries.

- `.deb`: Debian packages for Linux, when provided. To install:

    ```
    $ sudo dpkg -i <package file>
    ```

- `.zip`: ZIP-ed executable archive. Use your favorite or appropriate ZIP
  software to unzip the archive. The extracted archive's `bin`
  subdirectory is the location of the simulators.

- `.exe`: Nullsoft Scriptable Install System (NSIS)-created Windows
  installer.

  NOTE: The executable is not code-signed. DO NOT DOUBLE CLICK ON THE
  EXECUTABLE TO INSTALL. If you do, you are likely to get a Windows
  Defender popup box that will prevent you from installing ZIMH. Instead,
  use a CMD or PowerShell command window and execute the `.exe` from the
  command line prompt. For example, to install
  `zimh-<version>-win64-vs2026.exe`:

    ```
    ## PowerShell:
    PS> .\zimh-<version>-win64-vs2026.exe

    ## CMD:
    > .\zimh-<version>-win64-vs2026.exe
    ```

- `.msi`: WiX toolkit-created Windows MSI installer.

  These MSI files are not code-signed. DO NOT DOUBLE CLICK ON THE MSI TO
  INSTALL. If you do, you are likely to get a Windows Defender popup box
  that will prevent you from installing ZIMH. Instead, use a CMD or
  PowerShell command window to manually invoke `msiexec` to install ZIMH.
  From either a PowerShell or CMD command window:

    ```
    > msiexec /qf /i zimh-<version>-win64-vs2026.msi
    ```
