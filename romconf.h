/*
 * The ROM's machine-configuration block.
 */
#ifndef	ROMCONF_H
#define	ROMCONF_H
struct	romconf	{
	unsigned rom_bram;	/* beginning of RAM, 1 KB clicks */
	unsigned rom_eram;	/* end of RAM, 1 KB clicks */
	char	*rom_auto;	/* autoboot command (NULL = ask) */
	int	*rom_restart;	/* restart address */
	char	rom_fdtype;	/* floppy type */
	char	rom_hdtype;	/* hard-disk type (0 = none) */
	char	rom_ctype;	/* clock type: 0 = 4MHz, 1 = 6MHz */
	char	has_hires;	/* hi-res board fitted, its console used */
};
extern struct romconf romconf;	/* defined in rom.c */
#endif
