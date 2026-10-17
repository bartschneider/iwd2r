"""The Windows user profile that holds this project's Windows-side folders
(iwd2r-oracle, iwd2r-build, iwd2r-vm), as a WSL path.

$IWD2R_WIN_HOME when set (a /mnt/<drive>/... path); otherwise
/mnt/c/Users/<your WSL user name>, which is right when the Windows and WSL
accounts share a name. Set the variable when they do not, or when the profile
lives on another drive. Standard library only.
"""

import os


def win_home(env=None):
    """The profile folder as a WSL path, without a trailing slash."""
    env = os.environ if env is None else env
    explicit = (env.get('IWD2R_WIN_HOME') or '').rstrip('/')
    if explicit:
        return explicit
    user = (env.get('USER') or env.get('LOGNAME')
            or os.path.basename(os.path.expanduser('~')))
    return '/mnt/c/Users/' + user


WIN_HOME = win_home()
