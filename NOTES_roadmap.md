#5: IOCTL_MAP_FRAMEBUFFER and IOCTL_REFRESH_SCREEN to allow userspace access to the fb 
    depends on #4

#6: Implement pipes for 'term' to get processes stdout
    no dependencies, but useless until we do the framebuffer&input stuff
#7: Implement interfaces to write/delete files, create/remove directories, list dir contents etc
#8: Figure out a way for processes to share their framebuffer with the graphical server
    depends on everything else
#9: Implement every syscall used in newlib.c
