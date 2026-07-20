Import("env")
import subprocess
try:
    h = subprocess.check_output(["git","rev-parse","--short","HEAD"],
        cwd=env["PROJECT_DIR"], stderr=subprocess.DEVNULL).decode().strip()
except Exception:
    h = "nogit"
open(env["PROJECT_DIR"]+"/inc/version.h","w").write('#define GIT_HASH "%s"\n' % h)
print("version.h: GIT_HASH=%s" % h)
