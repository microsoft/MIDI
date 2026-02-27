:; set -eo pipefail
:; SCRIPT_DIR=$(cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd)
:; ${SCRIPT_DIR}/build.sh "$@"
:; exit $?

@ECHO OFF
set ServiceAndApiPlatformsAll=["x64"]
set InstallerPlatforms=["x64"]
powershell -ExecutionPolicy ByPass -NoProfile -File "%~dp0build.ps1" %*
