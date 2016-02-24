# ConcealDrive
Conceal a disk partition from Windows

This tool applies VeraCrypt XOR based concealing transformation to the first 8 KB of the selected device.
When applied the first time on a NTFS formatted drive, it will prevent Windows and applicatins from using it.
Applying it a second time reverts the concealing of the first 8 KB of the drive, making it usable as before.
