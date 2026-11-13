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
#if ROMKB > 32
	/* Valid when rc_magic reads "C9RC"; fields are only appended. */
	char	rc_magic[4];
	unsigned rc_version;
	unsigned rc_length;	/* bytes, from the start */
	unsigned rc_romver;	/* BCD */
	unsigned rc_video;	/* RV_LR | RV_HR | RV_LC */
	unsigned rc_hr404;	/* hi-res mode word */
	char	rc_lcid, rc_lcver;
	char	rc_console;	/* RC_CON_* */
	char	rc_conchan;	/* serial console port */
	unsigned rc_serial;	/* serial ports fitted, one bit each */
	unsigned rc_nram;
	struct { char seg, nseg; } rc_ram[16];	/* RAM runs, 64 KB segments */
	char	rc_badseg[32];	/* one bit per segment */
	unsigned rc_hdcyl;
	char	rc_hdheads, rc_hdsec;
	char	rc_bootdev[2];	/* "hd", "fd" */
	char	rc_bootunit, rc_pad;
	long	rc_bootoff;
	char	rc_rsvd_seg;	/* RAM the ROM uses until the program it loads runs */
	char	rc_rsvd_nseg;
	unsigned rc_config;
	unsigned rc_cksum;	/* words from rc_magic on sum to 0 */
	unsigned rc_net;	/* RN_LANCE */
	unsigned rc_mac[3];	/* LANCE address, first byte high */
#endif
};

#define RV_LR		0x01
#define RV_HR		0x02
#define RV_LC		0x04

#define RN_LANCE	0x01

#define RC_CON_SER	1
#define RC_CON_LR	3
#define RC_CON_HR	4
#define RC_CON_LC	5
extern struct romconf romconf;	/* defined in rom.c */
#endif
