/* Copyright (c) Daniel Thaler, 2011 */
/* DynaMoHack may be freely redistributed.  See license for details. */

#include "nhcurses.h"
#include <sys/stat.h>
#include <sys/types.h>
#include <ctype.h>

enum internal_commands {
    /* implicitly include enum nh_direction */
    UICMD_OPTIONS = DIR_SELF + 1,
    UICMD_EXTCMD,
    UICMD_HELP,
    UICMD_REDO,
    UICMD_REPEATPREFIX,
    UICMD_PREVMSG,
    UICMD_WHATDOES,
    UICMD_NOTHING
};

#define RESET_BINDINGS_ID (-10000)

#ifndef Ctrl
#define Ctrl(c)		(0x1f & (c))
#endif

#define DIRCMD		(1 << 29)
#define DIRCMD_SHIFT	(1 << 30)
#define DIRCMD_CTRL	(1 << 31)

struct nh_cmd_desc builtin_commands[] = {
    {"east",       "move, fight or interact to the east",	'l', 0, CMD_UI | DIRCMD | DIR_E},
    {"north",      "move, fight or interact to the north",	'k', 0, CMD_UI | DIRCMD | DIR_N},
    {"north_east", "move, fight or interact to the northeast",	'u', 0, CMD_UI | DIRCMD | DIR_NE},
    {"north_west", "move, fight or interact to the northwest",	'y', 0, CMD_UI | DIRCMD | DIR_NW},
    {"south",      "move, fight or interact to the south",	'j', 0, CMD_UI | DIRCMD | DIR_S},
    {"south_east", "move, fight or interact to the southeast",	'n', 0, CMD_UI | DIRCMD | DIR_SE},
    {"south_west", "move, fight or interact to the southwest",	'b', 0, CMD_UI | DIRCMD | DIR_SW},
    {"west",       "move, fight or interact to the west",	'h', 0, CMD_UI | DIRCMD | DIR_W},
    {"up",         "climb stairs or ladders",			'<', 0, CMD_UI | DIRCMD | DIR_UP},
    {"down",       "go down stairs or ladders or jump into holes", '>', 0, CMD_UI | DIRCMD | DIR_DOWN},
    
    {"run_east",       "go east until you run into something", 'L', 0, CMD_UI | DIRCMD_SHIFT | DIR_E},
    {"run_north",      "go north until you run into something", 'K', 0, CMD_UI | DIRCMD_SHIFT | DIR_N},
    {"run_north_east", "go northeast until you run into something", 'U', 0, CMD_UI | DIRCMD_SHIFT | DIR_NE},
    {"run_north_west", "go northwest until you run into something", 'Y', 0, CMD_UI | DIRCMD_SHIFT | DIR_NW},
    {"run_south",      "go south until you run into something", 'J', 0, CMD_UI | DIRCMD_SHIFT | DIR_S},
    {"run_south_east", "go southeast until you run into something", 'N', 0, CMD_UI | DIRCMD_SHIFT | DIR_SE},
    {"run_south_west", "go southwest until you run into something", 'B', 0, CMD_UI | DIRCMD_SHIFT | DIR_SW},
    {"run_west",       "go west until you run into something", 'H', 0, CMD_UI | DIRCMD_SHIFT | DIR_W},
    
    {"go_east",       "run east until something interesting is seen", Ctrl('l'), 0, CMD_UI | DIRCMD_CTRL | DIR_E},
    {"go_north",      "run north until something interesting is seen", Ctrl('k'), 0, CMD_UI | DIRCMD_CTRL | DIR_N},
    {"go_north_east", "run northeast until something interesting is seen", Ctrl('u'), 0, CMD_UI | DIRCMD_CTRL | DIR_NE},
    {"go_north_west", "run northwest until something interesting is seen", Ctrl('y'), 0, CMD_UI | DIRCMD_CTRL | DIR_NW},
    {"go_south",      "run south until something interesting is seen", Ctrl('j'), 0, CMD_UI | DIRCMD_CTRL | DIR_S},
    {"go_south_east", "run southeast until something interesting is seen", Ctrl('n'), 0, CMD_UI | DIRCMD_CTRL | DIR_SE},
    {"go_south_west", "run southwest until something interesting is seen", Ctrl('b'), 0, CMD_UI | DIRCMD_CTRL | DIR_SW},
    {"go_west",       "run west until something interesting is seen", Ctrl('h'), 0, CMD_UI | DIRCMD_CTRL | DIR_W}, /* nutty konsole sends KEY_BACKSPACE when ^H is pressed... */
    
    {"extcommand", "perform an extended command", '#', 0, CMD_UI | UICMD_EXTCMD},
    {"help",	   "show the help menu", '?', 0, CMD_UI | UICMD_HELP},
    {"options",	   "show option settings, possibly change them", 'O', 0, CMD_UI | UICMD_OPTIONS},
    {"prevmsg",	   "list previously displayed messages", Ctrl('p'), 0, CMD_UI | UICMD_PREVMSG},
    {"redo",	   "redo the previous command", '\001', 0, CMD_UI | UICMD_REDO},
    {"repeat_prefix", "show command repeat count prompt", 0, 0, CMD_UI | UICMD_REPEATPREFIX},
    {"whatdoes",   "describe what a key does", '&', 0, CMD_UI | UICMD_WHATDOES},
    {"(nothing)",  "bind keys to this command to suppress \"Bad command\".", 0, 0, CMD_UI | UICMD_NOTHING},
};


struct nh_cmd_desc *keymap[KEY_MAX], *unknown_keymap[KEY_MAX];
static struct nh_cmd_desc *commandlist, *unknown_commands;
static int cmdcount, unknown_count, unknown_size;
static struct nh_cmd_desc *prev_cmd;
static struct nh_cmd_arg prev_arg = {CMD_ARG_NONE}, next_command_arg;
static nh_bool prev_cmd_same = FALSE;
static int current_cmd_key;
static nh_bool have_next_command = FALSE;
static char next_command_name[32];
static int prev_count;

static void show_whatdoes(void);
static struct nh_cmd_desc *show_help(void);
static void init_keymap(void);
static struct nh_cmd_desc *doextcmd(void);


void reset_prev_cmd(void)
{
    prev_cmd = NULL;
    prev_cmd_same = FALSE;
}


nh_bool check_prev_cmd_same(void)
{
    return prev_cmd_same;
}


int get_current_cmd_key(void)
{
    return current_cmd_key;
}


const char *curses_keyname(int key)
{
    static char knbuf[16];
    const char *kname;
    if (key == ' ')
	return "SPACE";
    else if (key == '\033')
	return "ESC";
    
    /* if ncurses doesn't know a key, keyname() returns NULL.
     * This can happen if you create a keymap with pdcurses, and then read it with ncurses */
    kname = keyname(key);
    if (kname && strcmp(kname, "UNKNOWN KEY"))
	return kname;
    snprintf(knbuf, sizeof(knbuf), "KEY_#%d", key);
    return knbuf;
}


static struct nh_cmd_desc *find_command(const char *cmdname)
{
    int i, count;

    for (i = 0; i < cmdcount; i++)
	if (!strcmp(commandlist[i].name, cmdname))
	    return &commandlist[i];
    
    count = sizeof(builtin_commands)/sizeof(struct nh_cmd_desc);
    for (i = 0; i < count; i++)
	if (!strcmp(builtin_commands[i].name, cmdname))
	    return &builtin_commands[i];
    
    return NULL;
}


void handle_internal_cmd(struct nh_cmd_desc **cmd, struct nh_cmd_arg *arg, int *count)
{
    int id = (*cmd)->flags & ~(CMD_UI | DIRCMD | DIRCMD_SHIFT | DIRCMD_CTRL);
    switch (id) {
	case DIR_NW: case DIR_N: case DIR_NE:
	case DIR_E:              case DIR_W:
	case DIR_SW: case DIR_S: case DIR_SE:
	case DIR_UP: case DIR_DOWN:
	    arg->argtype = CMD_ARG_DIR;
	    arg->d = id;
	    if ((*cmd)->flags & DIRCMD)
		*cmd = find_command("move");
	    else if((*cmd)->flags & DIRCMD_SHIFT)
		*cmd = find_command("run");
	    else if((*cmd)->flags & DIRCMD_CTRL)
		*cmd = find_command("go2");
	    break;

	case UICMD_OPTIONS:
	    display_options(FALSE);
	    *cmd = NULL;
	    break;

	case UICMD_EXTCMD:
	    *cmd = doextcmd();
	    break;

	case UICMD_HELP:
	    arg->argtype = CMD_ARG_NONE;
	    *cmd = show_help();
	    break;

	case UICMD_REDO:
	    *cmd = prev_cmd;
	    *arg = prev_arg;
	    *count = prev_count;
	    break;

	case UICMD_REPEATPREFIX:
	    *cmd = NULL;
	    break;

	case UICMD_PREVMSG:
	    doprev_message();
	    *cmd = NULL;
	    break;

	case UICMD_WHATDOES:
	    show_whatdoes();
	    *cmd = NULL;
	    break;

	case UICMD_NOTHING:
	    *cmd = NULL;
	    break;
    }
}


const char *get_command(int *count, struct nh_cmd_arg *arg)
{
    int key, key2, multi;
    char line[BUFSZ];
    struct nh_cmd_desc *cmd, *cmd2;

    /* inventory item actions may set the next command */
    if (have_next_command) {
	have_next_command = FALSE;
	*count = 0;
	*arg = next_command_arg;
	return next_command_name;
    }

    do {
	multi = -1;
	cmd = NULL;
	arg->argtype = CMD_ARG_NONE;

	key = get_map_key(TRUE);
	if (key < 0 || key >= KEY_MAX)
	    continue;

	while ((multi == -1 && keymap[key] &&
		(keymap[key]->flags & (CMD_UI|UICMD_REPEATPREFIX)) == (CMD_UI|UICMD_REPEATPREFIX)) ||
	       ((multi >= 0 || settings.repeat_num_auto) && (key >= '0' && key <= '9')) ||
	       (multi >= 0 && (key == KEY_BACKSPACE || key == KEY_BACKDEL))) {

	    if (multi == -1)
		multi = 0;

	    if (key == KEY_BACKSPACE || key == KEY_BACKDEL) {
		/* backspace alters count unless bound to a command */
		if (!keymap[key] || (keymap[key]->flags & (CMD_UI|UICMD_NOTHING)) ==
				    (CMD_UI|UICMD_NOTHING)) {
		    if (multi > 0)
			multi /= 10;
		    else
			multi = -1;
		} else {
		    break;
		}
	    } else if (key >= '0' && key <= '9') {
		multi = 10 * multi + key - '0';
		if (multi > 0xffff)
		    multi /= 10;
	    }

	    do {
		if (multi >= 0) {
		    sprintf(line, "Count: %d", multi);
		    key = curses_msgwin(line);
		} else {
		    key = get_map_key(TRUE);
		}
	    } while (key < 0 || key >= KEY_MAX);
	}
	if (multi < 0)
	    multi = 0;

	if (key == '\033') /* filter out ESC */
	    continue;
	if (key < 0 || key >= KEY_MAX)
	    continue;

	new_action(); /* use a new message line for this action */
	*count = multi;
	cmd = keymap[key];
	current_cmd_key = key;
	
	if (cmd != NULL) {
	    /* handle internal commands. The command handler may alter
		* cmd, arg and count (redo does this) */
	    if (cmd->flags & CMD_UI) {
		handle_internal_cmd(&cmd, arg, count);
		if (!cmd) /* command was fully handled internally */
		    continue;
	    }

	    if (cmd == find_command("redraw")) {
		/* This needs special handling locally in addition to sending
		 * it to the server. */
		clear();
		rebuild_ui();
		doupdate();
	    }

	    /* if the command requres an arg AND the arg isn't set yet (by handle_internal_cmd) */
	    if (!(cmd->flags & CMD_ARG_NONE) && cmd->flags & CMD_ARG_DIR &&
		arg->argtype != CMD_ARG_DIR) {
		key2 = get_map_key(TRUE);
		if (key2 == '\033') /* cancel silently */
		    continue;
		if (key2 < 0 || key2 >= KEY_MAX)
		    continue;
		
		cmd2 = keymap[key2];
		if (cmd2 && (cmd2->flags & CMD_UI) && (cmd2->flags & DIRCMD)) {
		    arg->argtype = CMD_ARG_DIR;
		    arg->d = (enum nh_direction)(cmd2->flags & ~(CMD_UI | DIRCMD));
		} else
		    cmd = NULL;
	    }
	}
	
	if (!cmd) {
	    sprintf(line, "Bad command: '%s'.", curses_keyname(key));
	    curses_print_message(player.moves, line);
	}
    } while (!cmd);
    
    wmove(mapwin, player.y, player.x - 1);
    
    prev_cmd_same = (cmd == prev_cmd);
    prev_cmd = cmd;
    prev_arg = *arg;
    prev_count = *count;
    
    return cmd->name;
}


void set_next_command(const char *cmd, struct nh_cmd_arg *arg)
{
    have_next_command = TRUE;
    next_command_arg = *arg;
    strncpy(next_command_name, cmd, sizeof(next_command_name));
}


enum nh_direction key_to_dir(int key)
{
    struct nh_cmd_desc *cmd;

    if (key <= 0 || key >= KEY_MAX)
	return DIR_NONE;

    cmd = keymap[key];

    if (!cmd || !(cmd->flags & DIRCMD))
	return DIR_NONE;

    return (enum nh_direction) cmd->flags & ~(CMD_UI | DIRCMD);
}


/* here after #? - now list all full-word commands */
static int doextlist(const char **namelist, const char **desclist, int listlen)
{
    char buf[BUFSZ];
    int i, icount = 0, size = listlen;
    struct nh_menuitem *items = malloc(sizeof(struct nh_menuitem) * size);

    for (i = 0; i < listlen; i++) {
	    sprintf(buf, " %s\t- %s.", namelist[i], desclist[i]);
	    add_menu_txt(items, size, icount, buf, MI_TEXT);
    }
    curses_display_menu(items, icount, "Extended Commands List", PICK_NONE, NULL);

    return 0;
}


/* here after # - now read a full-word command */
static struct nh_cmd_desc *doextcmd(void)
{
    int i, idx, size;
    struct nh_cmd_desc *retval = NULL;
    char cmdbuf[BUFSZ];
    const char **namelist, **desclist;
    static const char exthelp[] = "?";
    int *idxmap;
    
    size = 0;
    for (i = 0; i < cmdcount; i++)
	if (commandlist[i].flags & CMD_EXT)
	    size++;
    namelist = malloc((size+1) * sizeof(char*));
    desclist = malloc((size+1) * sizeof(char*));
    idxmap = malloc((size+1) * sizeof(int));
    
    /* add help */
    namelist[size] = exthelp;
    desclist[size] = "get this list of extended commands";
    idxmap[size] = 0;
       
    idx = 0;
    for (i = 0; i < cmdcount; i++) {
	if (commandlist[i].flags & CMD_EXT) {
	    namelist[idx] = commandlist[i].name;
	    desclist[idx] = commandlist[i].desc;
	    idx++;
	}
    }
    
    /* keep repeating until we don't run help */
    do {
	if (!curses_get_ext_cmd(cmdbuf, namelist, desclist, size+1))
	    break;
	
	if (!strcmp(cmdbuf, exthelp)) {
	    doextlist(namelist, desclist, size+1);
	    continue;
	}
	
	retval = find_command(cmdbuf);
	
	/* don't allow ui commands: they wouldn't be handled properly later on */
	if (!retval || (retval->flags & CMD_UI)) {
	    char msg[BUFSZ];
	    retval = NULL;
	    sprintf(msg, "%s: unknown extended command.", cmdbuf);
	    curses_msgwin(msg);
	}
    } while (!retval);

    free(namelist);
    free(desclist);
    free(idxmap);
    
    return retval;
}


static void show_whatdoes(void)
{
    char buf[BUFSZ];
    int key = curses_msgwin("What command?");

    if (key < 0 || key >= KEY_MAX)
	return;

    if (!keymap[key])
	snprintf(buf, BUFSZ, "'%s' is not bound to any command.", curses_keyname(key));
    else
	snprintf(buf, BUFSZ, "'%s': %s - %s", curses_keyname(key), keymap[key]->name,
		 keymap[key]->desc);
    curses_msgwin(buf);
}


static void show_quickref(void)
{
    static struct nh_menuitem refitems[] = {
	{0, MI_HEADING, "Moving and Looking"},
	{0, MI_TEXT, "7 8 9   y k u"},
	{0, MI_TEXT, " \\|/     \\|/    < aim up/go upstairs"},
	{0, MI_TEXT, "4-.-6   h-.-l"},
	{0, MI_TEXT, " /|\\     /|\\    > aim down/go downstairs"},
	{0, MI_TEXT, "1 2 3   b j n"},
	{0, MI_TEXT, ""},
	{0, MI_TEXT, ".           wait (100. to wait 100 times)"},
	{0, MI_TEXT, "s           search surroundings (20s to search 20 times)"},
	{0, MI_TEXT, "m<dir>      move only"},
	{0, MI_TEXT, "F<dir>      fight only"},
	{0, MI_TEXT, "g<dir>      run (stop for anything)"},
	{0, MI_TEXT, "G<dir>/Ctrl-<dir>"},
	{0, MI_TEXT, "            run (along corridor ignoring intersections)"},
	{0, MI_TEXT, "Shift-<dir> run (along corridor until blocked)"},
	{0, MI_TEXT, "_           travel to targeted location, shortcuts:"},
	{0, MI_TEXT, "                <  upstairs"},
	{0, MI_TEXT, "                >  downstairs"},
	{0, MI_TEXT, "                _  altar"},
	{0, MI_TEXT, "v           autoexplore"},
	{0, MI_TEXT, ""},
	{0, MI_TEXT, ";           look"},
	{0, MI_TEXT, ":           look here"},
	{0, MI_TEXT, "/           multi-look or look up encyclopedia"},
	{0, MI_TEXT, ""},
	{0, MI_HEADING, "Meta-game"},
	{0, MI_TEXT, "?           view help"},
	{0, MI_TEXT, "O           view or change options"},
	{0, MI_TEXT, "S           save and exit"},
	{0, MI_TEXT, "#quit       END GAME PERMANENTLY"},
	{0, MI_TEXT, ""},
	{0, MI_HEADING, "Information"},
	{0, MI_TEXT, "Ctrl-X      character info"},
	{0, MI_TEXT, "Ctrl-P      previous messages"},
	{0, MI_TEXT, "Ctrl-O      dungeon overview"},
	{0, MI_TEXT, "\\           item discoveries"},
	{0, MI_TEXT, "C           call (name) item, monster or level"},
	{0, MI_TEXT, "#enhance    view or enhance skills"},
	{0, MI_TEXT, "+           view or reorder magic spells"},
	{0, MI_TEXT, "#history    record of achievements"},
	{0, MI_TEXT, "#conduct    voluntary challenges"},
	{0, MI_TEXT, ""},
	{0, MI_HEADING, "Items"},
	{0, MI_TEXT, "i           view or use inventory items"},
	{0, MI_TEXT, ",           pick up"},
	{0, MI_TEXT, "@           toggle autopickup"},
	{0, MI_TEXT, "d           drop (d3x drops 3 of item x)"},
	{0, MI_TEXT, "D           multi-drop"},
	{0, MI_TEXT, "#adjust     move item to another inventory slot"},
	{0, MI_TEXT, ""},
	{0, MI_TEXT, "w           wield weapon (w- to wield nothing)"},
	{0, MI_TEXT, "x           swap weapon to or from off-hand"},
	{0, MI_TEXT, "X           toggle two-weapon combat"},
	{0, MI_TEXT, "W/T         wear/take off armor, rings or amulet"},
	{0, MI_TEXT, "A           mass unequip"},
	{0, MI_TEXT, "t           throw item (wield bow and throw arrows to shoot)"},
	{0, MI_TEXT, "Q/f         quiver/fire quivered item (throwing shortcut)"},
	{0, MI_TEXT, "a           apply (use) tool"},
	{0, MI_TEXT, "e           eat food or corpse"},
	{0, MI_TEXT, "E           engrave (write) on floor (E- for fingers)"},
	{0, MI_TEXT, "Ctrl-E      engrave \"Elbereth\" (scares monsters)"},
	{0, MI_TEXT, "q           quaff (drink) potion"},
	{0, MI_TEXT, "r           read scroll or spellbook"},
	{0, MI_TEXT, "V           invoke artifact power or break wand"},
	{0, MI_TEXT, "z           zap wand"},
	{0, MI_TEXT, "#dip        dip item into potion or water"},
	{0, MI_TEXT, "#rub        rub an item"},
	{0, MI_TEXT, ""},
	{0, MI_HEADING, "Actions"},
	{0, MI_TEXT, "c           chat with pet or monster"},
	{0, MI_TEXT, "o           open or close door"},
	{0, MI_TEXT, "Ctrl-D      kick (e.g. locked door)"},
	{0, MI_TEXT, "p           pay for items (in shop)"},
	{0, MI_TEXT, "Z           cast magic spell"},
	{0, MI_TEXT, ""},
	{0, MI_TEXT, "#loot/a,    open container on floor"},
	{0, MI_TEXT, "#force      force open container with wielded weapon"},
	{0, MI_TEXT, "#tip        empty a container of its contents"},
	{0, MI_TEXT, "#untrap     find and disarm traps (e.g. container or door)"},
	{0, MI_TEXT, ""},
	{0, MI_TEXT, "#pray       pray for divine help (near-death or troubled)"},
	{0, MI_TEXT, "#sacrifice  sacrifice corpse or item at an altar"},
	{0, MI_TEXT, "#turn       turn (away) undead (knight or priest only)"},
	{0, MI_TEXT, ""},
	{0, MI_TEXT, "#jump       jump to nearby location (if able)"},
	{0, MI_TEXT, "#monster    use monster special ability (when polymorphed)"},
	{0, MI_TEXT, "#ride       mount or dismount saddled pet"},
	{0, MI_TEXT, "#sit        sit down (on throne/trap or to lay an egg)"},
	{0, MI_TEXT, "Ctrl-T      trigger teleportation (if able)"},
	{0, MI_TEXT, "#wipe       wipe eyes clean"},
    };

    curses_display_menu(refitems, ARRAY_SIZE(refitems), "Quick reference:",
			PICK_NONE, NULL);
}


static struct nh_cmd_desc* show_help(void)
{
    struct nh_menuitem *items;
    int i, n, size, icount, selected[1];

    size = 10;
    items = malloc(size * sizeof(struct nh_menuitem));
    icount = 0;

    add_menu_item(items, size, icount, 1, "describe game commands", 0, FALSE);
    add_menu_item(items, size, icount, 2, "describe what a key does", 0, FALSE);
    add_menu_item(items, size, icount, 3, "describe options", 0, FALSE);

    for (i = 0; i < cmdcount; i++)
	if (commandlist[i].flags & CMD_HELP)
	    add_menu_item(items, size, icount, 100+i, commandlist[i].desc, 0, FALSE);

    add_menu_txt(items, size, icount, "", MI_NORMAL);
    add_menu_item(items, size, icount, 4, "show quick reference", '?', FALSE);

    n = curses_display_menu(items, icount, "Help topics:", PICK_ONE, selected);
    free(items);
    if (n <= 0)
	return NULL;

    switch(selected[0]) {
	case 1:
	    show_keymap_menu(TRUE);
	    break;

	case 2:
	    show_whatdoes();
	    break;

	case 3:
	    print_options();
	    break;

	case 4:
	    show_quickref();
	    break;

	default:
	    n = selected[0] - 100;
	    if (n >= 0 && n < cmdcount)
		return &commandlist[n];
	    break;
    }

    return NULL;
}

/*----------------------------------------------------------------------------*/


/* read the user-configured keymap from keymap.conf.
 * Return TRUE if this succeeds, FALSE otherwise */
static nh_bool read_keymap(void)
{
    fnchar filename[BUFSZ];
    char *data, *line, *endptr;
    int fd, size, pos, key, i;
    struct nh_cmd_desc *cmd;
    nh_bool unknown;
    struct nh_cmd_desc *unknown_commands_prev;
    long ptrdiff;
    
    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
	return FALSE;
    fnncat(filename, FN("keymap.conf"), BUFSZ);

    fd = sys_open(filename, O_RDONLY, 0);
    if (fd == -1)
	return FALSE;
    
    size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    data = malloc(size + 1);
    read(fd, data, size);
    data[size] = '\0';
    close(fd);
    
    unknown_count = 0;
    memset(unknown_keymap, 0, sizeof(unknown_keymap));
    
    /* read the file */
    line = strtok(data, "\r\n");
    while (line) {
	/* find the first non-space after the first space (ie the second word) */
	pos = 0;
	while (line[pos] && !isspace((unsigned char)line[pos]))
	    pos++;
	while (line[pos] && isspace((unsigned char)line[pos]))
	    pos++;
	
	unknown = FALSE;
	cmd = find_command(&line[pos]);
	/* record unknown commands in the keymap: these may simply be valid, but
	 * unavailable in the current game. For example, the file might contain
	 * mappings for wizard mode commands.
	 */
	if (!cmd && line[pos] != '-') {
	    unknown = TRUE;
	    if (unknown_count >= unknown_size) {
		unknown_size = max(unknown_size * 2, 16);
		unknown_commands_prev = unknown_commands;
		unknown_commands = realloc(unknown_commands,
					unknown_size * sizeof(struct nh_cmd_desc));
		memset(&unknown_commands[unknown_count], 0,
		       sizeof(struct nh_cmd_desc) * (unknown_size - unknown_count));
		
		/* since unknown_commands has been realloc'd, pointers must be
		 * adjusted to point to the new list rather than free'd memory */
		ptrdiff = (char*)unknown_commands - (char*)unknown_commands_prev;
		for (i = 0; i < KEY_MAX; i++) {
		    if (!unknown_keymap[i])
			continue;
		    
		    unknown_keymap[i] = (void*)((char*)unknown_keymap[i] + ptrdiff);
		}
	    }
	    unknown_count++;
	    cmd = &unknown_commands[unknown_count-1];
	    strncpy(cmd->name, &line[pos], sizeof(cmd->name));
	    cmd->name[sizeof(cmd->name)-1] = '\0';
	}
	
	if (cmd) {
	    if (!strncmp(line, "EXT", 3))
		cmd->flags |= CMD_EXT;
	    else if(!strncmp(line, "NOEXT", 5))
		cmd->flags &= ~CMD_EXT;
	    else {
		key = strtol(line, &endptr, 16);
		if (key == 0 || endptr == line)
		    goto badmap;
		
		if (key < 0 || key >= KEY_MAX) /* manual edit or version difference */
		    goto nextline; /* nothing we can do with this, except perhaps complain */
		
		if (!unknown)
		    keymap[key] = cmd;
		else
		    unknown_keymap[key] = cmd;
	    }
	}
nextline:
	line = strtok(NULL, "\r\n");
    }
    
    
    free(data);
    return TRUE;
    
badmap:
    curses_msgwin("Bad/damaged keymap.conf. Reverting to defaults.");
    init_keymap();
    return FALSE;
}


/* store the keymap in keymap.conf */
static void write_keymap(void)
{
    int fd, i;
    unsigned int key;
    fnchar filename[BUFSZ];
    char buf[BUFSZ], *name;
    
    filename[0] = '\0';
    if (!get_gamedir(CONFIG_DIR, filename))
	return;
    fnncat(filename, FN("keymap.conf"), BUFSZ);

    fd = sys_open(filename, O_TRUNC | O_CREAT | O_RDWR, 0644);
    if (fd == -1)
	return;
#ifdef UNIX
    /* bypass umask and set 0644 for real */
    fchmod(fd, 0644);
#endif

    for (key = 1; key < KEY_MAX; key++) {
	name = keymap[key] ? keymap[key]->name :
	                (unknown_keymap[key] ? unknown_keymap[key]->name : "-");
	sprintf(buf, "%x %s\n", key, name);
	write(fd, buf, strlen(buf));
    }
    
    for (i = 0; i < cmdcount; i++) {
	if (commandlist[i].flags & CMD_EXT) {
	    sprintf(buf, "EXT %s\n", commandlist[i].name);
	    write(fd, buf, strlen(buf));
	} else {
	    sprintf(buf, "NOEXT %s\n", commandlist[i].name);
	    write(fd, buf, strlen(buf));
	}
    }
    
    for (i = 0; i < unknown_count; i++) {
	if (unknown_commands[i].flags & CMD_EXT) {
	    sprintf(buf, "EXT %s\n", unknown_commands[i].name);
	    write(fd, buf, strlen(buf));
	} else {
	    sprintf(buf, "NOEXT %s\n", unknown_commands[i].name);
	    write(fd, buf, strlen(buf));
	}
    }
    
    close(fd);
}


#define keytrans(x) ((unsigned int)(x))


/* initialize the keymap with the default keys suggested by the game */
static void init_keymap(void)
{
    int i;
    int count = sizeof(builtin_commands)/sizeof(struct nh_cmd_desc);
    
    memset(keymap, 0, sizeof(keymap));
    
    /* num pad direction keys */
    keymap[KEY_UP] = find_command("north");
    keymap[KEY_DOWN] = find_command("south");
    keymap[KEY_LEFT] = find_command("west");
    keymap[KEY_RIGHT] = find_command("east");
    keymap[KEY_A1] = find_command("north_west");
    keymap[KEY_A3] = find_command("north_east");
    keymap[KEY_C1] = find_command("south_west");
    keymap[KEY_C3] = find_command("south_east");
    /* diagonal keypad keys are not necessarily reported as A1, A3, C1, C3 */
    keymap[KEY_HOME]  = find_command("north_west");
    keymap[KEY_PPAGE] = find_command("north_east");
    keymap[KEY_END]   = find_command("south_west");
    keymap[KEY_NPAGE] = find_command("south_east");
    keymap['\r'] = find_command("(nothing)");
    
    /* every command automatically gets its default key */
    for (i = 0; i < cmdcount; i++)
	if (commandlist[i].defkey)
	    keymap[keytrans(commandlist[i].defkey)] = &commandlist[i];
	
    for (i = 0; i < count; i++)
	if (builtin_commands[i].defkey)
	    keymap[keytrans(builtin_commands[i].defkey)] = &builtin_commands[i];
    
    /* alt keys are assigned if the key is not in use */
    for (i = 0; i < cmdcount; i++) {
	if (commandlist[i].altkey && !keymap[keytrans(commandlist[i].altkey)])
	    keymap[keytrans(commandlist[i].altkey)] = &commandlist[i];
    }
    
    for (i = 0; i < count; i++) {
	if (builtin_commands[i].altkey &&
	    !keymap[keytrans(commandlist[i].altkey)])
	    keymap[keytrans(builtin_commands[i].altkey)] = &builtin_commands[i];
    }
    
}


void load_keymap(void)
{
    struct nh_cmd_desc *cmdlist = nh_get_commands(&cmdcount);
    
    commandlist = malloc(cmdcount * sizeof(struct nh_cmd_desc));
    memcpy(commandlist, cmdlist, cmdcount * sizeof(struct nh_cmd_desc));

    /* always init the keymap - read keymap might not set up every mapping */
    init_keymap();
    read_keymap();
}


void free_keymap(void)
{
    free(commandlist);
    commandlist = NULL;
    cmdcount = 0;
    
    if (unknown_commands) {
	free(unknown_commands);
	unknown_commands = NULL;
	unknown_size = unknown_count = 0;
    }
}


/* add the description of a command to the keymap menu */
static void add_keylist_command(struct nh_cmd_desc *cmd,
				struct nh_menuitem *item, int id)
{
    char buf[BUFSZ];
    char keys[BUFSZ];
    int i, kl;
    
    keys[0] = '\0';
    for (i = 0; i < KEY_MAX; i++) {
	if (keymap[i] == cmd) {
	    kl = strlen(keys);
	    if (kl) {
		keys[kl++] = ' ';
		keys[kl] = '\0';
	    }
	    strncat(keys, curses_keyname(i), BUFSZ - kl - 1);
	    keys[BUFSZ-1] = '\0';
	}
    }
    
    sprintf(buf, "%s%.15s\t%.50s\t%.16s", cmd->flags & CMD_EXT ? "#" : "",
	    cmd->name, cmd->desc, keys);
    set_menuitem(item, id, MI_NORMAL, buf, 0, FALSE);
}


/* display a menu to alter the key bindings for the given command */
static void command_settings_menu(struct nh_cmd_desc *cmd)
{
    char buf[BUFSZ];
    int i, n, size = 10, icount, selection[1];
    struct nh_menuitem *items = malloc(sizeof(struct nh_menuitem) * size);
    
    do {
	icount = 0;
	for (i = 0; i < KEY_MAX; i++) {
	    if (keymap[i] == cmd) {
		sprintf(buf, "delete key %s", curses_keyname(i));
		add_menu_item(items, size, icount, i, buf, 0, FALSE);
	    }
	}
	if (icount > 0)
	    add_menu_txt(items, size, icount, "", MI_NORMAL);
	add_menu_item(items, size, icount, -1, "Add a new key", '+', FALSE);
	if (!(cmd->flags & CMD_UI)) {
	    if (cmd->flags & CMD_EXT)
		add_menu_item(items, size, icount, -2,
			      "Don't use as an extended command", 0, FALSE);
	    else
		add_menu_item(items, size, icount, -2,
			      "Use as an extended command", 0, FALSE);
	}
	
	sprintf(buf, "Key bindings for %s", cmd->name);
	n = curses_display_menu(items, icount, buf, PICK_ONE, selection);
	if (n < 1)
	    break;
	
	/* int this menu, ids > 0 are used for "delete key" items and id is the
	 * actual key. Negative ids are used for the 2 static menu items */
	if (selection[0] > 0) /* delete a key */
	    keymap[selection[0]] = NULL;
	else if (selection[0] == -1) { /* add a key */
	    sprintf(buf, "Press the key you want to use for \"%s\"", cmd->name);
	    i = curses_msgwin(buf);
	    if (i == KEY_ESC || i < 0 || i >= KEY_MAX)
		continue;
	    if (keymap[i]) {
		sprintf(buf, "That key is already in use by \"%s\"! Replace?", keymap[i]->name);
		if ('y' != curses_yn_function(buf, "yn", 'n'))
		    continue;
	    }
	    keymap[i] = cmd;
	    
	} else if (selection[0] == -2) { /* toggle extended command status */
	    cmd->flags = (cmd->flags ^ CMD_EXT);
	}
	    
    } while (n > 0);
    
    free(items);
}


static nh_bool set_command_keys(struct win_menu *mdat, int idx)
{
    int id = mdat->items[idx].id;
    struct nh_cmd_desc *cmd, *cmdlist;
    
    if (id == RESET_BINDINGS_ID) {
	int i, count = 0;
	init_keymap(); /* fully reset the keymap */
	
	/* reset extcmds */
	cmdlist = nh_get_commands(&count);
	for (i = 0; i < count; i++) {
	    cmd = find_command(cmdlist[i].name);
	    if (cmd)
		cmd->flags = cmdlist[i].flags;
	}
	
	return TRUE;
    }
    
    if (id < 0)
	cmd = &builtin_commands[-(id+1)];
    else
	cmd = &commandlist[id-1];
    
    command_settings_menu(cmd);
    
    return TRUE;
}


void show_keymap_menu(nh_bool readonly)
{
    int i, n, icount;
    nh_bool need_init = !cmdcount;
    struct nh_menuitem *items;

    if (need_init)
	load_keymap();

    items = malloc(sizeof(struct nh_menuitem) *
                                 (ARRAY_SIZE(builtin_commands) + cmdcount + 4));
    
    do {
	set_menuitem(&items[0], 0, MI_HEADING, "Command\tDescription\tKey", 0, FALSE);
	icount = 1;
	/* add builtin commands */
	for (i = 0; i < ARRAY_SIZE(builtin_commands); i++) {
	    add_keylist_command(&builtin_commands[i], &items[icount],
				readonly ? 0 : -(i+1));
	    icount++;
	}
	
	/* add in-game commands */
	for (i = 0; i < cmdcount; i++) {
	    add_keylist_command(&commandlist[i], &items[icount],
				readonly ? 0 : (i+1));
	    icount++;
	}
	
	if (!readonly) {
	    set_menuitem(&items[icount++], 0, MI_TEXT, "", 0, FALSE);
	    set_menuitem(&items[icount++], RESET_BINDINGS_ID, MI_NORMAL,
			"!!!\tReset all key bindings to built-in defaults\t!!!", '!', FALSE);
	}
	n = curses_display_menu_core(items, icount, "Keymap", readonly ? PICK_NONE :
				     PICK_ONE, NULL, 0, 0, COLS, LINES,
				     set_command_keys, FALSE);
    } while(n > 0);
    free(items);
    
    write_keymap();

    if (need_init)
	free_keymap();
}
