# arch-diff
Perform a full diff between all vanilla packages installed by pacman and the file system.

## Example usage:

    $ sudo arch-diff
    [modified]  uid 124 != 0: /var/lib/colord/icc
    [modified]  mode 644 != 600: /etc/cups/classes.conf
    [modified]  md5 96633ac97e54318a8a7a546097eff55b != 66a2e629e353f50b15a3a554d38df61a: /etc/cups/printers.conf
    [modified]  mode 644 != 640: /etc/cups/subscriptions.conf
    [modified]  size 93 != 161: /etc/dmd.conf
    ...
    [modified]  size 1069 != 1149: /etc/timidity++/timidity.cfg
    [modified]  mode 755 != 644: /usr/share/vibed/source/vibe/crypto/cryptorand.d
    [modified]  size 284329 != 279213: /usr/lib/vlc/plugins/plugins.dat
    [untracked] /boot/efi/
    [untracked] /boot/initramfs-linux-fallback.img
    [untracked] /boot/initramfs-linux.img
    [untracked] /boot/refind_linux.conf
    [untracked] /etc/.pwd.lock
    [untracked] /etc/.updated
    [untracked] /etc/X11/xorg.conf.d/10-keyboard.conf
    [untracked] /etc/X11/xorg.conf.d/42-dualscreen.conf
    ...
    [untracked] /var/net-snmp/
      416136 tracked
         369 untracked
           0 missing
          47 modified
