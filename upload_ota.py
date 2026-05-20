Import("env")
import sys
sys.path.insert(0, env["PROJECT_DIR"])

try:
    import pio_secrets
    _host = getattr(pio_secrets, 'OTA_HOST', 'muhpark.local')
    _auth = getattr(pio_secrets, 'OTA_PASS', None)
except ImportError:
    _host = 'muhpark.local'
    _auth = None

# Set port early (used as $UPLOAD_PORT in the platform's command template)
env.Replace(UPLOAD_PORT=_host)

def _before_upload(source, target, env):
    if not _auth:
        return
    # The platform's builder/main.py runs after this script and replaces
    # UPLOADERFLAGS (including --auth ""). This callback fires right before
    # the upload command so we can safely patch the auth at the last moment.
    flags = [str(f) for f in env.get("UPLOADERFLAGS", [])]
    cleaned, skip = [], False
    for f in flags:
        if skip:
            skip = False
            continue
        if f == "--auth":
            skip = True   # drop --auth and the empty value after it
        elif f.startswith("--auth="):
            pass          # drop --auth=
        else:
            cleaned.append(f)
    env.Replace(UPLOADERFLAGS=cleaned + ["--auth", _auth])

env.AddPreAction("upload",   _before_upload)
env.AddPreAction("uploadfs", _before_upload)
