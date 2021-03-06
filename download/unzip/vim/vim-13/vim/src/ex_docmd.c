/* vi:set ts=8 sts=4 sw=4:
 *
 * VIM - Vi IMproved	by Bram Moolenaar
 *
 * Do ":help uganda"  in Vim to read copying and usage conditions.
 * Do ":help credits" in Vim to see a list of people who contributed.
 * See README.txt for an overview of the Vim source code.
 */

/*
 * ex_docmd.c: functions for executing an Ex command line.
 */

#include "vim.h"

#ifdef HAVE_FCNTL_H
# include <fcntl.h>	    /* for chdir() */
#endif

static int	quitmore = 0;
static int	ex_pressedreturn = FALSE;
#ifndef FEAT_PRINTER
# define ex_hardcopy	ex_ni
#endif

#ifdef FEAT_USR_CMDS
typedef struct ucmd
{
    char_u	*uc_name;	/* The command name */
    long_u	uc_argt;	/* The argument type */
    char_u	*uc_rep;	/* The command's replacement string */
    long	uc_def;		/* The default value for a range/count */
    scid_T	uc_scriptID;	/* SID where the command was defined */
    int		uc_compl;	/* completion type */
    char_u	*uc_compl_arg;	/* completion argument if any */
} ucmd_T;

#define UC_BUFFER	1	/* -buffer: local to current buffer */

garray_T ucmds = {0, 0, sizeof(ucmd_T), 4, NULL};

#define USER_CMD(i) (&((ucmd_T *)(ucmds.ga_data))[i])
#define USER_CMD_GA(gap, i) (&((ucmd_T *)((gap)->ga_data))[i])

static void do_ucmd __ARGS((exarg_T *eap));
static void ex_command __ARGS((exarg_T *eap));
static void ex_comclear __ARGS((exarg_T *eap));
static void ex_delcommand __ARGS((exarg_T *eap));
# ifdef FEAT_CMDL_COMPL
static char_u *get_user_command_name __ARGS((int idx));
# endif

#else
# define ex_command	ex_ni
# define ex_comclear	ex_ni
# define ex_delcommand	ex_ni
#endif

#ifdef FEAT_EVAL
static void	free_cmdlines __ARGS((garray_T *gap));
static char_u	*do_one_cmd __ARGS((char_u **, int, struct condstack *, char_u *(*getline)(int, void *, int), void *cookie));
#else
static char_u	*do_one_cmd __ARGS((char_u **, int, char_u *(*getline)(int, void *, int), void *cookie));
static int	if_level = 0;		/* depth in :if */
#endif
static int	checkforcmd __ARGS((char_u **pp, char *cmd, int len));
static char_u	*find_command __ARGS((exarg_T *eap, int *full));

static void	ex_abbreviate __ARGS((exarg_T *eap));
static void	ex_map __ARGS((exarg_T *eap));
static void	ex_unmap __ARGS((exarg_T *eap));
static void	ex_mapclear __ARGS((exarg_T *eap));
static void	ex_abclear __ARGS((exarg_T *eap));
#ifndef FEAT_MENU
# define ex_emenu		ex_ni
# define ex_menu		ex_ni
# define ex_menutranslate	ex_ni
#endif
#ifdef FEAT_AUTOCMD
static void	ex_autocmd __ARGS((exarg_T *eap));
static void	ex_doautocmd __ARGS((exarg_T *eap));
#else
# define ex_autocmd		ex_ni
# define ex_doautocmd		ex_ni
# define ex_doautoall		ex_ni
#endif
#ifdef FEAT_LISTCMDS
static void	ex_bunload __ARGS((exarg_T *eap));
static void	ex_buffer __ARGS((exarg_T *eap));
static void	ex_bmodified __ARGS((exarg_T *eap));
static void	ex_bnext __ARGS((exarg_T *eap));
static void	ex_bprevious __ARGS((exarg_T *eap));
static void	ex_brewind __ARGS((exarg_T *eap));
static void	ex_blast __ARGS((exarg_T *eap));
#else
# define ex_bunload		ex_ni
# define ex_buffer		ex_ni
# define ex_bmodified		ex_ni
# define ex_bnext		ex_ni
# define ex_bprevious		ex_ni
# define ex_brewind		ex_ni
# define ex_blast		ex_ni
# define buflist_list		ex_ni
# define ex_checktime		ex_ni
#endif
#if !defined(FEAT_LISTCMDS) || !defined(FEAT_WINDOWS)
# define ex_buffer_all		ex_ni
#endif
static char_u	*getargcmd __ARGS((char_u **));
static char_u	*skip_cmd_arg __ARGS((char_u *p, int rembs));
static int	getargopt __ARGS((exarg_T *eap));
#ifndef FEAT_QUICKFIX
# define ex_make		ex_ni
# define ex_cc			ex_ni
# define ex_cnext		ex_ni
# define ex_cfile		ex_ni
# define qf_list		ex_ni
# define qf_age			ex_ni
# define ex_helpgrep		ex_ni
#endif
#if !defined(FEAT_QUICKFIX) || !defined(FEAT_WINDOWS)
# define ex_cclose		ex_ni
# define ex_copen		ex_ni
# define ex_cwindow		ex_ni
#endif

static int	check_more __ARGS((int, int));
static linenr_T get_address __ARGS((char_u **, int skip, int otherfile));
static char_u	*invalid_range __ARGS((exarg_T *eap));
static void	correct_range __ARGS((exarg_T *eap));
static char_u	*repl_cmdline __ARGS((exarg_T *eap, char_u *src, int srclen, char_u *repl, char_u **cmdlinep));
static void	ex_highlight __ARGS((exarg_T *eap));
static void	ex_colorscheme __ARGS((exarg_T *eap));
static void	ex_quit __ARGS((exarg_T *eap));
static void	ex_cquit __ARGS((exarg_T *eap));
static void	ex_quit_all __ARGS((exarg_T *eap));
#ifdef FEAT_WINDOWS
static void	ex_close __ARGS((exarg_T *eap));
static void	ex_win_close __ARGS((exarg_T *eap, win_T *win));
static void	ex_only __ARGS((exarg_T *eap));
static void	ex_all __ARGS((exarg_T *eap));
static void	ex_resize __ARGS((exarg_T *eap));
static void	ex_stag __ARGS((exarg_T *eap));
#else
# define ex_close		ex_ni
# define ex_only		ex_ni
# define ex_all			ex_ni
# define ex_resize		ex_ni
# define ex_splitview		ex_ni
# define ex_stag		ex_ni
#endif
#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
static void	ex_pclose __ARGS((exarg_T *eap));
static void	ex_ptag __ARGS((exarg_T *eap));
static void	ex_pedit __ARGS((exarg_T *eap));
#else
# define ex_pclose		ex_ni
# define ex_ptag		ex_ni
# define ex_pedit		ex_ni
#endif
static void	ex_hide __ARGS((exarg_T *eap));
static void	ex_stop __ARGS((exarg_T *eap));
static void	ex_exit __ARGS((exarg_T *eap));
static void	ex_print __ARGS((exarg_T *eap));
#ifdef FEAT_BYTEOFF
static void	ex_goto __ARGS((exarg_T *eap));
#else
# define ex_goto		ex_ni
#endif
static void	ex_shell __ARGS((exarg_T *eap));
static void	ex_preserve __ARGS((exarg_T *eap));
static void	ex_recover __ARGS((exarg_T *eap));
#ifndef FEAT_LISTCMDS
# define ex_argedit		ex_ni
# define ex_argadd		ex_ni
# define ex_argdelete		ex_ni
# define ex_listdo		ex_ni
#endif
static void	ex_mode __ARGS((exarg_T *eap));
static void	ex_wrongmodifier __ARGS((exarg_T *eap));
static void	ex_find __ARGS((exarg_T *eap));
static void	ex_edit __ARGS((exarg_T *eap));
#if !defined(FEAT_GUI) && !defined(FEAT_CLIENTSERVER)
# define ex_drop		ex_ni
#endif
#ifndef FEAT_GUI
# define ex_gui			ex_nogui
static void	ex_nogui __ARGS((exarg_T *eap));
#endif
#if defined(FEAT_GUI_W32) && defined(FEAT_MENU) && defined(FEAT_TEAROFF)
static void	ex_tearoff __ARGS((exarg_T *eap));
#else
# define ex_tearoff		ex_ni
#endif
#if (defined(FEAT_GUI_MSWIN) || defined(FEAT_GUI_GTK)) && defined(FEAT_MENU)
static void	ex_popup __ARGS((exarg_T *eap));
#else
# define ex_popup		ex_ni
#endif
#ifndef FEAT_GUI_MSWIN
# define ex_simalt		ex_ni
#endif
#if !defined(FEAT_GUI_MSWIN) && !defined(FEAT_GUI_GTK) && !defined(FEAT_GUI_MOTIF)
# define gui_mch_find_dialog	ex_ni
# define gui_mch_replace_dialog ex_ni
#endif
#ifndef FEAT_GUI_GTK
# define ex_helpfind		ex_ni
#endif
#ifndef FEAT_CSCOPE
# define do_cscope		ex_ni
# define do_scscope		ex_ni
# define do_cstag		ex_ni
#endif
#ifndef FEAT_SYN_HL
# define ex_syntax		ex_ni
#endif
#ifndef FEAT_PERL
# define ex_perl		ex_ni
# define ex_perldo		ex_ni
#endif
#ifndef FEAT_PYTHON
# define ex_python		ex_ni
# define ex_pyfile		ex_ni
#endif
#ifndef FEAT_TCL
# define ex_tcl			ex_ni
# define ex_tcldo		ex_ni
# define ex_tclfile		ex_ni
#endif
#ifndef FEAT_RUBY
# define ex_ruby		ex_ni
# define ex_rubydo		ex_ni
# define ex_rubyfile		ex_ni
#endif
#ifndef FEAT_SNIFF
# define ex_sniff		ex_ni
#endif
#ifndef FEAT_KEYMAP
# define ex_loadkeymap		ex_ni
#endif
static void	ex_swapname __ARGS((exarg_T *eap));
static void	ex_syncbind __ARGS((exarg_T *eap));
static void	ex_read __ARGS((exarg_T *eap));
static void	ex_cd __ARGS((exarg_T *eap));
static void	ex_pwd __ARGS((exarg_T *eap));
static void	ex_equal __ARGS((exarg_T *eap));
static void	ex_sleep __ARGS((exarg_T *eap));
static void	do_exmap __ARGS((exarg_T *eap, int isabbrev));
static void	ex_winsize __ARGS((exarg_T *eap));
#ifdef FEAT_WINDOWS
static void	ex_wincmd __ARGS((exarg_T *eap));
#else
# define ex_wincmd	    ex_ni
#endif
#if defined(FEAT_GUI) || defined(UNIX) || defined(VMS)
static void	ex_winpos __ARGS((exarg_T *eap));
#else
# define ex_winpos	    ex_ni
#endif
static void	ex_operators __ARGS((exarg_T *eap));
static void	ex_put __ARGS((exarg_T *eap));
static void	ex_copymove __ARGS((exarg_T *eap));
static void	ex_submagic __ARGS((exarg_T *eap));
static void	ex_join __ARGS((exarg_T *eap));
static void	ex_at __ARGS((exarg_T *eap));
static void	ex_bang __ARGS((exarg_T *eap));
static void	ex_undo __ARGS((exarg_T *eap));
static void	ex_redo __ARGS((exarg_T *eap));
static void	ex_redir __ARGS((exarg_T *eap));
static void	ex_redraw __ARGS((exarg_T *eap));
static void	close_redir __ARGS((void));
static void	ex_mkrc __ARGS((exarg_T *eap));
static void	ex_mark __ARGS((exarg_T *eap));
#ifdef FEAT_USR_CMDS
static char_u	*uc_fun_cmd __ARGS((void));
#endif
#ifdef FEAT_EX_EXTRA
static void	ex_normal __ARGS((exarg_T *eap));
static void	ex_startinsert __ARGS((exarg_T *eap));
static void	ex_stopinsert __ARGS((exarg_T *eap));
#else
# define ex_normal		ex_ni
# define ex_align		ex_ni
# define ex_retab		ex_ni
# define ex_startinsert		ex_ni
# define ex_stopinsert		ex_ni
# define ex_helptags		ex_ni
#endif
#ifdef FEAT_FIND_ID
static void	ex_checkpath __ARGS((exarg_T *eap));
static void	ex_findpat __ARGS((exarg_T *eap));
#else
# define ex_findpat		ex_ni
# define ex_checkpath		ex_ni
#endif
#if defined(FEAT_FIND_ID) && defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
static void	ex_psearch __ARGS((exarg_T *eap));
#else
# define ex_psearch		ex_ni
#endif
static void	ex_tag __ARGS((exarg_T *eap));
static void	ex_tag_cmd __ARGS((exarg_T *eap, char_u *name));
#ifndef FEAT_EVAL
# define ex_scriptnames		ex_ni
# define ex_finish		ex_ni
# define ex_echo		ex_ni
# define ex_echohl		ex_ni
# define ex_execute		ex_ni
# define ex_call		ex_ni
# define ex_if			ex_ni
# define ex_endif		ex_ni
# define ex_else		ex_ni
# define ex_while		ex_ni
# define ex_continue		ex_ni
# define ex_break		ex_ni
# define ex_endwhile		ex_ni
# define ex_throw		ex_ni
# define ex_try			ex_ni
# define ex_catch		ex_ni
# define ex_finally		ex_ni
# define ex_endtry		ex_ni
# define ex_endfunction		ex_ni
# define ex_let			ex_ni
# define ex_unlet		ex_ni
# define ex_function		ex_ni
# define ex_delfunction		ex_ni
# define ex_return		ex_ni
#endif
static char_u	*arg_all __ARGS((void));
#ifdef FEAT_SESSION
static int	makeopens __ARGS((FILE *fd, char_u *dirnow));
static int	put_view __ARGS((FILE *fd, win_T *wp, int add_edit, unsigned *flagp));
static void	ex_loadview __ARGS((exarg_T *eap));
static char_u	*get_view_file __ARGS((int c));
#else
# define ex_loadview		ex_ni
#endif
#ifndef FEAT_EVAL
# define ex_compiler		ex_ni
#endif
#ifdef FEAT_VIMINFO
static void	ex_viminfo __ARGS((exarg_T *eap));
#else
# define ex_viminfo		ex_ni
#endif
static void	ex_behave __ARGS((exarg_T *eap));
#ifdef FEAT_AUTOCMD
static void	ex_filetype __ARGS((exarg_T *eap));
static void	ex_setfiletype  __ARGS((exarg_T *eap));
#else
# define ex_filetype		ex_ni
# define ex_setfiletype		ex_ni
#endif
#ifndef FEAT_DIFF
# define ex_diffpatch		ex_ni
# define ex_diffgetput		ex_ni
# define ex_diffsplit		ex_ni
# define ex_diffthis		ex_ni
# define ex_diffupdate		ex_ni
#endif
static void	ex_digraphs __ARGS((exarg_T *eap));
static void	ex_set __ARGS((exarg_T *eap));
#if !defined(FEAT_EVAL) || !defined(FEAT_AUTOCMD)
# define ex_options		ex_ni
#endif
#ifdef FEAT_SEARCH_EXTRA
static void	ex_nohlsearch __ARGS((exarg_T *eap));
static void	ex_match __ARGS((exarg_T *eap));
#else
# define ex_nohlsearch		ex_ni
# define ex_match		ex_ni
#endif
#ifdef FEAT_CRYPT
static void	ex_X __ARGS((exarg_T *eap));
#else
# define ex_X			ex_ni
#endif
#ifdef FEAT_FOLDING
static void	ex_fold __ARGS((exarg_T *eap));
static void	ex_foldopen __ARGS((exarg_T *eap));
static void	ex_folddo __ARGS((exarg_T *eap));
#else
# define ex_fold		ex_ni
# define ex_foldopen		ex_ni
# define ex_folddo		ex_ni
#endif
#if !((defined(HAVE_LOCALE_H) || defined(X_LOCALE)) \
	&& (defined(FEAT_GETTEXT) || defined(FEAT_MBYTE)))
# define ex_language		ex_ni
#endif
#ifndef FEAT_SIGNS
# define ex_sign		ex_ni
#endif
#ifndef FEAT_SUN_WORKSHOP
# define ex_wsverb		ex_ni
#endif

#ifndef FEAT_EVAL
# define ex_debug		ex_ni
# define ex_breakadd		ex_ni
# define ex_debuggreedy		ex_ni
# define ex_breakdel		ex_ni
# define ex_breaklist		ex_ni
#endif

#ifndef FEAT_CMDHIST
# define ex_history		ex_ni
#endif
#ifndef FEAT_JUMPLIST
# define ex_jumps		ex_ni
#endif

/*
 * Declare cmdnames[].
 */
#define DO_DECLARE_EXCMD
#include "ex_cmds.h"

/*
 * Table used to quickly search for a command, based on its first character.
 */
cmdidx_T cmdidxs[27] =
{
	CMD_append,
	CMD_buffer,
	CMD_change,
	CMD_delete,
	CMD_edit,
	CMD_file,
	CMD_global,
	CMD_help,
	CMD_insert,
	CMD_join,
	CMD_k,
	CMD_list,
	CMD_move,
	CMD_next,
	CMD_open,
	CMD_print,
	CMD_quit,
	CMD_read,
	CMD_substitute,
	CMD_t,
	CMD_undo,
	CMD_vglobal,
	CMD_write,
	CMD_xit,
	CMD_yank,
	CMD_z,
	CMD_bang
};

static char_u dollar_command[2] = {'$', 0};


/*
 * do_exmode(): Repeatedly get commands for the "Ex" mode, until the ":vi"
 * command is given.
 */
    void
do_exmode(improved)
    int		improved;	    /* TRUE for "improved Ex" mode */
{
    int		save_msg_scroll;
    int		prev_msg_row;
    linenr_T	prev_line;

    save_msg_scroll = msg_scroll;
    ++RedrawingDisabled;	    /* don't redisplay the window */
    ++no_wait_return;		    /* don't wait for return */
    if (improved)
	exmode_active = EXMODE_VIM;
    else
    {
	settmode(TMODE_COOK);
	exmode_active = EXMODE_NORMAL;
    }

    State = NORMAL;
#ifdef FEAT_GUI
    /* Ignore scrollbar and mouse events in Ex mode */
    ++hold_gui_events;
#endif
#ifdef FEAT_SNIFF
    want_sniff_request = 0;    /* No K_SNIFF wanted */
#endif

    MSG(_("Entering Ex mode.  Type \"visual\" to go to Normal mode."));
    while (exmode_active)
    {
	msg_scroll = TRUE;
	need_wait_return = FALSE;
	ex_pressedreturn = FALSE;
	ex_no_reprint = FALSE;
	prev_msg_row = msg_row;
	prev_line = curwin->w_cursor.lnum;
#ifdef FEAT_SNIFF
	ProcessSniffRequests();
#endif
	if (improved)
	{
	    cmdline_row = msg_row;
	    do_cmdline(NULL, getexline, NULL, 0);
	}
	else
	    do_cmdline(NULL, getexmodeline, NULL, DOCMD_NOWAIT);
	lines_left = Rows - 1;

	if (prev_line != curwin->w_cursor.lnum && !ex_no_reprint)
	{
	    if (ex_pressedreturn)
	    {
		/* go up one line, to overwrite the ":<CR>" line, so the
		 * output doensn't contain empty lines. */
		msg_row = prev_msg_row;
		if (prev_msg_row == Rows - 1)
		    msg_row--;
	    }
	    msg_col = 0;
	    print_line_no_prefix(curwin->w_cursor.lnum, FALSE);
	    msg_clr_eos();
	}
	else if (ex_pressedreturn)	/* must be at EOF */
	    EMSG(_("E501: At end-of-file"));
    }

#ifdef FEAT_GUI
    --hold_gui_events;
#endif
    if (!improved)
	settmode(TMODE_RAW);
    --RedrawingDisabled;
    --no_wait_return;
    update_screen(CLEAR);
    need_wait_return = FALSE;
    msg_scroll = save_msg_scroll;
}

/*
 * Execute a simple command line.  Used for translated commands like "*".
 */
    int
do_cmdline_cmd(cmd)
    char_u	*cmd;
{
    return do_cmdline(cmd, NULL, NULL,
				   DOCMD_VERBOSE|DOCMD_NOWAIT|DOCMD_KEYTYPED);
}

/* Struct for storing a line inside a while loop */
typedef struct
{
    char_u	*line;		/* command line */
    linenr_T	lnum;		/* sourcing_lnum of the line */
} wcmd_T;

/*
 * do_cmdline(): execute one Ex command line
 *
 * 1. Execute "cmdline" when it is not NULL.
 *    If "cmdline" is NULL, or more lines are needed, getline() is used.
 * 2. Split up in parts separated with '|'.
 *
 * This function can be called recursively!
 *
 * flags:
 * DOCMD_VERBOSE  - The command will be included in the error message.
 * DOCMD_NOWAIT   - Don't call wait_return() and friends.
 * DOCMD_REPEAT   - Repeat execution until getline() returns NULL.
 * DOCMD_KEYTYPED - Don't reset KeyTyped.
 * DOCMD_EXCRESET - Reset the exception environment (used for debugging).
 *
 * return FAIL if cmdline could not be executed, OK otherwise
 */
    int
do_cmdline(cmdline, getline, cookie, flags)
    char_u	*cmdline;
    char_u	*(*getline) __ARGS((int, void *, int));
    void	*cookie;		/* argument for getline() */
    int		flags;
{
    char_u	*next_cmdline;		/* next cmd to execute */
    char_u	*cmdline_copy = NULL;	/* copy of cmd line */
    static int	recursive = 0;		/* recursive depth */
    int		msg_didout_before_start = 0;
    int		count = 0;		/* line number count */
    int		did_inc = FALSE;	/* incremented RedrawingDisabled */
    int		retval = OK;
#ifdef FEAT_EVAL
    struct condstack cstack;		/* conditional stack */
    garray_T	lines_ga;		/* keep lines for ":while" */
    int		current_line = 0;	/* active line in lines_ga */
    int		saved_trylevel = 0;
    int		saved_force_abort = 0;
    except_T	*saved_caught_stack = NULL;
    char_u	*saved_vv_exception = NULL;
    char_u	*saved_vv_throwpoint = NULL;
    int		saved_did_emsg = 0;
    int		saved_got_int = 0;
    int		saved_did_throw = 0;
    int		saved_need_rethrow = 0;
    int		saved_check_cstack = 0;
    except_T	*saved_current_exception = NULL;
    int		initial_trylevel;
    struct msglist	**saved_msg_list = NULL;
    struct msglist	*private_msg_list;
#endif
    static int	call_depth = 0;		/* recursiveness */

#ifdef FEAT_EVAL
    /* For every pair of do_cmdline()/do_one_cmd() calls, use an extra memory
     * location for storing error messages to be converted to an exception.
     * This ensures that the do_errthrow() call in do_one_cmd() does not combine
     * the messages stored by an earlier invocation of do_one_cmd() with the
     * command name of the later one.  This would happen when BufWritePost
     * autocommands are executed after a write error. */
    saved_msg_list = msg_list;
    msg_list = &private_msg_list;
    private_msg_list = NULL;
#endif

    /* It's possible to create an endless loop with ":execute", catch that
     * here.  The value of 200 allows nested function calls, ":source", etc. */
    if (call_depth == 200)
    {
	EMSG(_("E169: Command too recursive"));
#ifdef FEAT_EVAL
	/* When converting to an exception, we do not include the command name
	 * since this is not an error of the specific command. */
	do_errthrow((struct condstack *)NULL, (char_u *)NULL);
#endif
	return FAIL;
    }
    ++call_depth;

#ifdef FEAT_EVAL
    cstack.cs_idx = -1;
    cstack.cs_whilelevel = 0;
    cstack.cs_trylevel = 0;
    cstack.cs_emsg_silent_list = NULL;
    cstack.cs_had_while = FALSE;
    cstack.cs_had_endwhile = FALSE;
    cstack.cs_had_continue = FALSE;
    cstack.cs_had_finally = FALSE;
    ga_init2(&lines_ga, (int)sizeof(wcmd_T), 10);

    /* Inside a function use a higher nesting level. */
    if (getline == get_func_line && ex_nesting_level == func_level(cookie))
	++ex_nesting_level;

    /*
     * Initialize "force_abort"  and "suppress_errthrow" at the top level.
     */
    if (!recursive)
    {
	force_abort = FALSE;
	suppress_errthrow = FALSE;
    }

    /*
     * If requested, store and reset the global values controlling the
     * exception handling (used when debugging).
     */
    else if (flags & DOCMD_EXCRESET)
    {
	saved_trylevel		= trylevel;		trylevel = 0;
	saved_force_abort	= force_abort;		force_abort = FALSE;
	saved_caught_stack	= caught_stack;		caught_stack = NULL;
	saved_vv_exception	= v_exception(NULL);
	saved_vv_throwpoint	= v_throwpoint(NULL);
	/* Necessary for debugging an inactive ":catch", ":finally", or
	 * ":endtry": */
	saved_did_emsg		= did_emsg,		did_emsg     = FALSE;
	saved_got_int		= got_int,		got_int	     = FALSE;
	saved_did_throw		= did_throw,		did_throw    = FALSE;
	saved_need_rethrow	= need_rethrow,		need_rethrow = FALSE;
	saved_check_cstack	= check_cstack,		check_cstack = FALSE;
	saved_current_exception = current_exception;	current_exception=NULL;
    }

    initial_trylevel = trylevel;

    /*
     * "did_throw" will be set to TRUE when an exception is being thrown.
     */
    did_throw = FALSE;
#endif
    /*
     * "did_emsg" will be set to TRUE when emsg() is used, in which case we
     * cancel the whole command line, and any if/endif while/endwhile loop.
     * If force_abort is set, we cancel everything.
     */
    did_emsg = FALSE;

    /*
     * KeyTyped is only set when calling vgetc().  Reset it here when not
     * calling vgetc() (sourced command lines).
     */
    if (!(flags & DOCMD_KEYTYPED) && getline != getexline)
	KeyTyped = FALSE;

    /*
     * Continue executing command lines:
     * - when inside an ":if" or ":while"
     * - for multiple commands on one line, separated with '|'
     * - when repeating until there are no more lines (for ":source")
     */
    next_cmdline = cmdline;
    do
    {
	/* stop skipping cmds for an error msg after all endifs and endwhiles */
	if (next_cmdline == NULL
#ifdef FEAT_EVAL
		&& !force_abort
		&& cstack.cs_idx < 0
		&& !(getline == get_func_line && func_has_abort(cookie))
#endif
							)
	    did_emsg = FALSE;

	/*
	 * 1. If repeating a line with ":while", get a line from lines_ga.
	 * 2. If no line given: Get an allocated line with getline().
	 * 3. If a line is given: Make a copy, so we can mess with it.
	 */

#ifdef FEAT_EVAL
	/* 1. If repeating, get a previous line from lines_ga. */
	if (cstack.cs_whilelevel && current_line < lines_ga.ga_len)
	{
	    /* Each '|' separated command is stored separately in lines_ga, to
	     * be able to jump to it.  Don't use next_cmdline now. */
	    vim_free(cmdline_copy);
	    cmdline_copy = NULL;

	    /* Check if a function has returned or, unless it has an unclosed
	     * try conditional, aborted. */
	    if (getline == get_func_line && func_has_ended(cookie))
	    {
		retval = FAIL;
		break;
	    }

	    /* Check if a sourced file hit a ":finish" command. */
	    if (getline == getsourceline && source_finished(cookie))
	    {
		retval = FAIL;
		break;
	    }

	    next_cmdline = ((wcmd_T *)(lines_ga.ga_data))[current_line].line;
	    sourcing_lnum = ((wcmd_T *)(lines_ga.ga_data))[current_line].lnum;
	}
#endif

	/* 2. If no line given, get an allocated line with getline(). */
	if (next_cmdline == NULL)
	{
	    /*
	     * Need to set msg_didout for the first line after an ":if",
	     * otherwise the ":if" will be overwritten.
	     */
	    if (count == 1 && getline == getexline)
		msg_didout = TRUE;
	    if (getline == NULL || (next_cmdline = getline(':', cookie,
#ifdef FEAT_EVAL
		    cstack.cs_idx < 0 ? 0 : (cstack.cs_idx + 1) * 2
#else
		    0
#endif
			    )) == NULL)
	    {
		/* Don't call wait_return for aborted command line.  The NULL
		 * returned for the end of a sourced file or executed function
		 * doesn't do this. */
		if (KeyTyped && !(flags & DOCMD_REPEAT))
		    need_wait_return = FALSE;
		retval = FAIL;
		break;
	    }
	}

	/* 3. Make a copy of the command so we can mess with it. */
	else if (cmdline_copy == NULL)
	{
	    next_cmdline = vim_strsave(next_cmdline);
	    if (next_cmdline == NULL)
	    {
		EMSG(_(e_outofmem));
		retval = FAIL;
		break;
	    }
	}
	cmdline_copy = next_cmdline;

#ifdef FEAT_EVAL
	/*
	 * Save the current line when inside a ":while", and when the command
	 * looks like a ":while", because we may need it later.
	 * When there is a '|' and another command, it is stored separately,
	 * because we need to be able to jump back to it from an :endwhile.
	 */
	if (	   current_line == lines_ga.ga_len
		&& (cstack.cs_whilelevel || has_while_cmd(next_cmdline)))
	{
	    if (ga_grow(&lines_ga, 1) == FAIL)
	    {
		EMSG(_(e_outofmem));
		retval = FAIL;
		break;
	    }
	    ((wcmd_T *)(lines_ga.ga_data))[current_line].line =
						    vim_strsave(next_cmdline);
	    ((wcmd_T *)(lines_ga.ga_data))[current_line].lnum = sourcing_lnum;
	    ++lines_ga.ga_len;
	    --lines_ga.ga_room;
	}
	did_endif = FALSE;
#endif

	if (count++ == 0)
	{
	    /*
	     * All output from the commands is put below each other, without
	     * waiting for a return. Don't do this when executing commands
	     * from a script or when being called recursive (e.g. for ":e
	     * +command file").
	     */
	    if (!(flags & DOCMD_NOWAIT) && !recursive)
	    {
		msg_didout_before_start = msg_didout;
		msg_didany = FALSE; /* no output yet */
		msg_start();
		msg_scroll = TRUE;  /* put messages below each other */
		++no_wait_return;   /* dont wait for return until finished */
		++RedrawingDisabled;
		did_inc = TRUE;
	    }
	}

	if (p_verbose >= 15 && sourcing_name != NULL)
	{
	    int	c = -1;

	    ++no_wait_return;
	    msg_scroll = TRUE;	    /* always scroll up, don't overwrite */
	    /* Truncate long lines, smsg() can't handle that. */
	    if (STRLEN(cmdline_copy) > 200)
	    {
		c = cmdline_copy[200];
		cmdline_copy[200] = NUL;
	    }
	    smsg((char_u *)_("line %ld: %s"),
					   (long)sourcing_lnum, cmdline_copy);
	    msg_puts((char_u *)"\n");   /* don't overwrite this either */
	    if (c >= 0)
		cmdline_copy[200] = c;
	    cmdline_row = msg_row;
	    --no_wait_return;
	}

	/*
	 * 2. Execute one '|' separated command.
	 *    do_one_cmd() will return NULL if there is no trailing '|'.
	 *    "cmdline_copy" can change, e.g. for '%' and '#' expansion.
	 */
	++recursive;
	next_cmdline = do_one_cmd(&cmdline_copy, flags & DOCMD_VERBOSE,
#ifdef FEAT_EVAL
				&cstack,
#endif
				getline, cookie);
	--recursive;
	if (next_cmdline == NULL)
	{
	    vim_free(cmdline_copy);
	    cmdline_copy = NULL;
#ifdef FEAT_CMDHIST
	    /*
	     * If the command was typed, remember it for the ':' register.
	     * Do this AFTER executing the command to make :@: work.
	     */
	    if (getline == getexline && new_last_cmdline != NULL)
	    {
		vim_free(last_cmdline);
		last_cmdline = new_last_cmdline;
		new_last_cmdline = NULL;
	    }
#endif
	}
	else
	{
	    /* need to copy the command after the '|' to cmdline_copy, for the
	     * next do_one_cmd() */
	    mch_memmove(cmdline_copy, next_cmdline, STRLEN(next_cmdline) + 1);
	    next_cmdline = cmdline_copy;
	}


#ifdef FEAT_EVAL
	/* reset did_emsg for a function that is not aborted by an error */
	if (did_emsg && !force_abort
		&& getline == get_func_line && !func_has_abort(cookie))
	    did_emsg = FALSE;

	if (cstack.cs_whilelevel)
	{
	    ++current_line;

	    /*
	     * An ":endwhile" and ":continue" is handled here.
	     * If we were executing commands, jump back to the ":while".
	     * If we were not executing commands, decrement whilelevel.
	     */
	    if (cstack.cs_had_endwhile || cstack.cs_had_continue)
	    {
		if (cstack.cs_had_endwhile)
		    cstack.cs_had_endwhile = FALSE;
		else
		    cstack.cs_had_continue = FALSE;

		/* Jump back to the matching ":while".  Be careful not to use
		 * a cs_line[] from an entry that isn't a ":while": It would
		 * make "current_line" invalid and can cause a crash. */
		if (!did_emsg && !got_int && !did_throw
			&& cstack.cs_idx >= 0
			&& (cstack.cs_flags[cstack.cs_idx] & CSF_WHILE)
			&& cstack.cs_line[cstack.cs_idx] >= 0
			&& (cstack.cs_flags[cstack.cs_idx] & CSF_ACTIVE))
		{
		    current_line = cstack.cs_line[cstack.cs_idx];
		    cstack.cs_had_while = TRUE;	/* note we jumped there */
		    line_breakcheck();		/* check if CTRL-C typed */
		}
		else /* can only get here with ":endwhile" */
		{
		    --cstack.cs_whilelevel;
		    if (cstack.cs_idx >= 0)
			--cstack.cs_idx;
		}
	    }

	    /*
	     * For a ":while" we need to remember the line number.
	     */
	    else if (cstack.cs_had_while)
	    {
		cstack.cs_had_while = FALSE;
		cstack.cs_line[cstack.cs_idx] = current_line - 1;
	    }
	}

	/*
	 * When not inside any ":while" loop, clear remembered lines.
	 */
	if (!cstack.cs_whilelevel)
	{
	    if (lines_ga.ga_len > 0)
	    {
		sourcing_lnum =
		       ((wcmd_T *)lines_ga.ga_data)[lines_ga.ga_len - 1].lnum;
		free_cmdlines(&lines_ga);
	    }
	    current_line = 0;
	}

	/*
	 * A ":finally" makes did_emsg, got_int, and did_throw pending for being
	 * restored at the ":endtry".  Reset them here and set the ACTIVE and
	 * FINALLY flags, so that the finally clause gets executed.  This
	 * includes the case where a missing ":endif" or ":endwhile" was
	 * detected by the ":finally" itself.
	 */
	if (cstack.cs_had_finally)
	{
	    cstack.cs_had_finally = FALSE;
	    report_make_pending(cstack.cs_pending[cstack.cs_idx] &
		    (CSTP_ERROR | CSTP_INTERRUPT | CSTP_THROW),
		    did_throw ? (void *)current_exception : NULL);
	    did_emsg = got_int = did_throw = FALSE;
	    cstack.cs_flags[cstack.cs_idx] |= CSF_ACTIVE | CSF_FINALLY;
	}

	/* Update global "trylevel" for recursive calls to do_cmdline() from
	 * within this loop. */
	trylevel = initial_trylevel + cstack.cs_trylevel;

	/*
	 * If the outermost try conditional (accross function calls and sourced
	 * files) is aborted because of an error, an interrupt, or an uncaught
	 * exception, cancel everything.  If it is left normally, reset
	 * force_abort to get the non-EH compatible abortion behavior for
	 * the rest of the script.
	 */
	if (trylevel == 0 && !did_emsg && !got_int && !did_throw)
	    force_abort = FALSE;

	/* Convert an interrupt to an exception if appropriate. */
	(void)do_intthrow(&cstack);
#endif /* FEAT_EVAL */

    }
    /*
     * Continue executing command lines when:
     * - no CTRL-C typed, no aborting error, no exception thrown or try
     *   conditionals need to be checked for executing finally clauses or
     *   catching an interrupt exception
     * - didn't get an error message or lines are not typed
     * - there is a command after '|', inside a :if, :while, or :try, or
     *   looping for ":source" command or function call.
     */
    while (!((got_int
#ifdef FEAT_EVAL
		    || (did_emsg && force_abort) || did_throw
#endif
	     )
#ifdef FEAT_EVAL
		&& cstack.cs_trylevel == 0
#endif
	    )
	    && !(did_emsg && (getline == getexmodeline || getline == getexline))
	    && (next_cmdline != NULL
#ifdef FEAT_EVAL
			|| cstack.cs_idx >= 0
#endif
			|| (flags & DOCMD_REPEAT)));

    vim_free(cmdline_copy);
#ifdef FEAT_EVAL
    free_cmdlines(&lines_ga);
    ga_clear(&lines_ga);

    if (cstack.cs_idx >= 0)
    {
	/*
	 * If a sourced file or executed function ran to its end, report the
	 * unclosed conditional.
	 */
	if (!got_int && !did_throw
		&& ((getline == getsourceline && !source_finished(cookie))
		    || (getline == get_func_line && !func_has_ended(cookie))))
	{
	    if (cstack.cs_flags[cstack.cs_idx] & CSF_TRY)
		EMSG(_(e_endtry));
	    else if (cstack.cs_flags[cstack.cs_idx] & CSF_WHILE)
		EMSG(_(e_endwhile));
	    else
		EMSG(_(e_endif));
	}

	/*
	 * Reset "trylevel" in case of a ":finish" or ":return" or a missing
	 * ":endtry" in a sourced file or executed function.  If the try
	 * conditional is in its finally clause, ignore anything pending.
	 * If it is in a catch clause, finish the caught exception.
	 */
	do
	    cstack.cs_idx = cleanup_conditionals(&cstack, 0, TRUE);
	while (--cstack.cs_idx >= 0);
	trylevel = initial_trylevel;
    }

    /* If a missing ":endtry", ":endwhile", or ":endif" or a memory lack
     * was reported above and the error message is to be converted to an
     * exception, do this now after rewinding the cstack. */
    do_errthrow(&cstack, getline == get_func_line
				  ? (char_u *)"endfunction" : (char_u *)NULL);

    if (trylevel == 0)
    {
	/*
	 * When an exception is being thrown out of the outermost try
	 * conditional, discard the uncaught exception, disable the conversion
	 * of interrupts or errors to exceptions, and ensure that no more
	 * commands are executed.
	 */
	if (did_throw)
	{
	    void	*p = NULL;
	    char_u	*saved_sourcing_name;
	    int		saved_sourcing_lnum;
	    struct msglist	*messages = NULL, *next;

	    /*
	     * If the uncaught exception is a user exception, report it as an
	     * error.  If it is an error exception, display the saved error
	     * message now.  For an interrupt exception, do nothing; the
	     * interrupt message is given elsewhere.
	     */
	    switch (current_exception->type)
	    {
		case ET_USER:
		    sprintf((char *)IObuff, _("E605: Exception not caught: %s"),
			    current_exception->value);
		    p = vim_strsave(IObuff);
		    break;
		case ET_ERROR:
		    messages = current_exception->messages;
		    current_exception->messages = NULL;
		    break;
		case ET_INTERRUPT:
		    break;
		default:
		    p = vim_strsave((char_u *)_(e_internal));
	    }

	    saved_sourcing_name = sourcing_name;
	    saved_sourcing_lnum = sourcing_lnum;
	    sourcing_name = current_exception->throw_name;
	    sourcing_lnum = current_exception->throw_lnum;
	    current_exception->throw_name = NULL;

	    discard_current_exception();	/* uses IObuff if 'verbose' */
	    suppress_errthrow = TRUE;
	    force_abort = TRUE;

	    if (messages != NULL)
	    {
		do
		{
		    next = messages->next;
		    emsg(messages->msg);
		    vim_free(messages->msg);
		    vim_free(messages);
		    messages = next;
		}
		while (messages != NULL);
	    }
	    else if (p != NULL)
	    {
		emsg(p);
		vim_free(p);
	    }
	    sourcing_name = saved_sourcing_name;
	    sourcing_lnum = saved_sourcing_lnum;
	}

	/*
	 * On an interrupt or an aborting error not converted to an exception,
	 * disable the conversion of errors to exceptions.  (Interrupts are not
	 * converted any more, here.) This enables also the interrupt message
	 * when force_abort is set and did_emsg unset in case of an interrupt
	 * from a finally clause after an error.
	 */
	else if (got_int || (did_emsg && force_abort))
	    suppress_errthrow = TRUE;
    }

    /*
     * The current cstack will be freed when do_cmdline() returns.  An uncaught
     * exception will have to be rethrown in the previous cstack.  If a function
     * has just returned or a script file was just finished and the previous
     * cstack belongs to the same function or, respectively, script file, it
     * will have to be checked for finally clauses to be executed due to the
     * ":return" or ":finish".  This is done in do_one_cmd().
     */
    if (did_throw)
	need_rethrow = TRUE;
    if ((getline == getsourceline
		&& ex_nesting_level > source_level(cookie))
	    || (getline == get_func_line
		&& ex_nesting_level > func_level(cookie) + 1))
    {
	if (!did_throw)
	    check_cstack = TRUE;
    }
    else
    {
	/* When leaving a function, reduce nesting level. */
	if (getline == get_func_line)
	    --ex_nesting_level;
	/*
	 * Go to debug mode when returning from a function in which we are
	 * single-stepping.
	 */
	if ((getline == getsourceline || getline == get_func_line)
		&& ex_nesting_level + 1 <= debug_break_level)
	    do_debug(getline == getsourceline
		    ? (char_u *)_("End of sourced file")
		    : (char_u *)_("End of function"));
    }

    /*
     * Restore the exception environment (done after returning from the
     * debugger).
     */
    if (flags & DOCMD_EXCRESET)
    {
	suppress_errthrow = FALSE;
	trylevel = saved_trylevel;
	force_abort = saved_force_abort;
	caught_stack = saved_caught_stack;
	(void)v_exception(saved_vv_exception);
	(void)v_throwpoint(saved_vv_throwpoint);
	/* Necessary for debugging an inactive ":catch", ":finally", or
	 * ":endtry": */
	did_emsg = saved_did_emsg;
	got_int = saved_got_int;
	did_throw = saved_did_throw;
	need_rethrow = saved_need_rethrow;
	check_cstack = saved_check_cstack;
	current_exception = saved_current_exception;
    }

    msg_list = saved_msg_list;
#endif /* FEAT_EVAL */

    /*
     * If there was too much output to fit on the command line, ask the user to
     * hit return before redrawing the screen. With the ":global" command we do
     * this only once after the command is finished.
     */
    if (did_inc)
    {
	--RedrawingDisabled;
	--no_wait_return;
	msg_scroll = FALSE;

	/*
	 * When just finished an ":if"-":else" which was typed, no need to
	 * wait for hit-return.  Also for an error situation.
	 */
	if (retval == FAIL
#ifdef FEAT_EVAL
		|| (did_endif && KeyTyped && !did_emsg)
#endif
					    )
	{
	    need_wait_return = FALSE;
	    msg_didany = FALSE;		/* don't wait when restarting edit */
	}
	else if (need_wait_return)
	{
	    /*
	     * The msg_start() above clears msg_didout. The wait_return we do
	     * here should not overwrite the command that may be shown before
	     * doing that.
	     */
	    msg_didout |= msg_didout_before_start;
	    wait_return(FALSE);
	}
    }

#ifndef FEAT_EVAL
    /*
     * Reset if_level, in case a sourced script file contains more ":if" than
     * ":endif" (could be ":if x | foo | endif").
     */
    if_level = 0;
#endif

    --call_depth;
    return retval;
}

#ifdef FEAT_EVAL
    static void
free_cmdlines(gap)
    garray_T	*gap;
{
    while (gap->ga_len > 0)
    {
	vim_free(((wcmd_T *)(gap->ga_data))[gap->ga_len - 1].line);
	--gap->ga_len;
	++gap->ga_room;
    }
}
#endif

/*
 * Execute one Ex command.
 *
 * If 'sourcing' is TRUE, the command will be included in the error message.
 *
 * 1. skip comment lines and leading space
 * 2. handle command modifiers
 * 3. parse range
 * 4. parse command
 * 5. parse arguments
 * 6. switch on command name
 *
 * Note: "getline" can be NULL.
 *
 * This function may be called recursively!
 */
#if (_MSC_VER == 1200)
/*
 * Optimisation bug in VC++ version 6.0
 * TODO: check this is still present after each service pack
 */
#pragma optimize( "g", off )
#endif
    static char_u *
do_one_cmd(cmdlinep, sourcing,
#ifdef FEAT_EVAL
			    cstack,
#endif
				    getline, cookie)
    char_u		**cmdlinep;
    int			sourcing;
#ifdef FEAT_EVAL
    struct condstack	*cstack;
#endif
    char_u		*(*getline) __ARGS((int, void *, int));
    void		*cookie;		/* argument for getline() */
{
    char_u		*p;
    linenr_T		lnum;
    long		n;
    char_u		*errormsg = NULL;	/* error message */
    exarg_T		ea;			/* Ex command arguments */
    long		verbose_save = -1;
    int			save_msg_scroll = 0;
    int			did_silent = 0;
    int			did_esilent = 0;
    cmdmod_T		save_cmdmod;
    int			ni;			/* set when Not Implemented */

    vim_memset(&ea, 0, sizeof(ea));
    ea.line1 = 1;
    ea.line2 = 1;
#ifdef FEAT_EVAL
    ++ex_nesting_level;
#endif

	/* when not editing the last file :q has to be typed twice */
    if (quitmore
#ifdef FEAT_EVAL
	    /* avoid that a function call in 'statusline' does this */
	    && getline != get_func_line
#endif
	    )
	--quitmore;

    /*
     * Reset browse, confirm, etc..  They are restored when returning, for
     * recursive calls.
     */
    save_cmdmod = cmdmod;
    vim_memset(&cmdmod, 0, sizeof(cmdmod));

    /*
     * Repeat until no more command modifiers are found.
     */
    ea.cmd = *cmdlinep;
    for (;;)
    {
/*
 * 1. skip comment lines and leading white space and colons
 */
	while (*ea.cmd == ' ' || *ea.cmd == '\t' || *ea.cmd == ':')
	    ++ea.cmd;

	/* in ex mode, an empty line works like :+ */
	if (*ea.cmd == NUL && exmode_active
			&& (getline == getexmodeline || getline == getexline))
	{
	    ea.cmd = (char_u *)"+";
	    ex_pressedreturn = TRUE;
	}

	/* ignore comment and empty lines */
	if (*ea.cmd == '"' || *ea.cmd == NUL)
	    goto doend;

/*
 * 2. handle command modifiers.
 */
	p = ea.cmd;
	if (isdigit(*ea.cmd))
	    p = skipwhite(skipdigits(ea.cmd));
	switch (*p)
	{
	    case 'a':	if (!checkforcmd(&ea.cmd, "aboveleft", 3))
			    break;
#ifdef FEAT_WINDOWS
			cmdmod.split |= WSP_ABOVE;
#endif
			continue;

	    case 'b':	if (checkforcmd(&ea.cmd, "belowright", 3))
			{
#ifdef FEAT_WINDOWS
			    cmdmod.split |= WSP_BELOW;
#endif
			    continue;
			}
			if (checkforcmd(&ea.cmd, "browse", 3))
			{
#ifdef FEAT_BROWSE
			    cmdmod.browse = TRUE;
#endif
			    continue;
			}
			if (!checkforcmd(&ea.cmd, "botright", 2))
			    break;
#ifdef FEAT_WINDOWS
			cmdmod.split |= WSP_BOT;
#endif
			continue;

	    case 'c':	if (!checkforcmd(&ea.cmd, "confirm", 4))
			    break;
#if defined(FEAT_GUI_DIALOG) || defined(FEAT_CON_DIALOG)
			cmdmod.confirm = TRUE;
#endif
			continue;

			/* ":hide" and ":hide | cmd" are not modifiers */
	    case 'h':	if (p != ea.cmd || !checkforcmd(&p, "hide", 3)
					       || *p == NUL || ends_excmd(*p))
			    break;
			ea.cmd = p;
			cmdmod.hide = TRUE;
			continue;

	    case 'l':	if (!checkforcmd(&ea.cmd, "leftabove", 5))
			    break;
#ifdef FEAT_WINDOWS
			cmdmod.split |= WSP_ABOVE;
#endif
			continue;

	    case 'r':	if (!checkforcmd(&ea.cmd, "rightbelow", 6))
			    break;
#ifdef FEAT_WINDOWS
			cmdmod.split |= WSP_BELOW;
#endif
			continue;

	    case 's':	if (!checkforcmd(&ea.cmd, "silent", 3))
			    break;
			++did_silent;
			++msg_silent;
			save_msg_scroll = msg_scroll;
			if (*ea.cmd == '!' && !vim_iswhite(ea.cmd[-1]))
			{
			    /* ":silent!", but not "silent !cmd" */
			    ea.cmd = skipwhite(ea.cmd + 1);
			    ++emsg_silent;
			    ++did_esilent;
			}
			continue;

	    case 't':	if (!checkforcmd(&ea.cmd, "topleft", 2))
			    break;
#ifdef FEAT_WINDOWS
			cmdmod.split |= WSP_TOP;
#endif
			continue;

	    case 'v':	if (checkforcmd(&ea.cmd, "vertical", 4))
			{
#ifdef FEAT_VERTSPLIT
			    cmdmod.split |= WSP_VERT;
#endif
			    continue;
			}
			if (!checkforcmd(&p, "verbose", 4))
			    break;
			if (verbose_save < 0)
			    verbose_save = p_verbose;
			p_verbose = atoi((char *)ea.cmd);
			if (p_verbose == 0)
			    p_verbose = 1;
			ea.cmd = p;
			continue;
	}
	break;
    }

#ifdef FEAT_EVAL
    ea.skip = did_emsg || got_int || did_throw || (cstack->cs_idx >= 0
			 && !(cstack->cs_flags[cstack->cs_idx] & CSF_ACTIVE));
#else
    ea.skip = (if_level > 0);
#endif

#ifdef FEAT_EVAL
    /* May go to debug mode.  If this happens and the ">quit" debug command is
     * used, throw an interrupt exception and skip the next command. */
    dbg_check_breakpoint(&ea);
    if (!ea.skip && got_int)
    {
	ea.skip = TRUE;
	(void)do_intthrow(cstack);
    }
#endif

/*
 * 3. parse a range specifier of the form: addr [,addr] [;addr] ..
 *
 * where 'addr' is:
 *
 * %	      (entire file)
 * $  [+-NUM]
 * 'x [+-NUM] (where x denotes a currently defined mark)
 * .  [+-NUM]
 * [+-NUM]..
 * NUM
 *
 * The ea.cmd pointer is updated to point to the first character following the
 * range spec. If an initial address is found, but no second, the upper bound
 * is equal to the lower.
 */

    /* repeat for all ',' or ';' separated addresses */
    for (;;)
    {
	ea.line1 = ea.line2;
	ea.line2 = curwin->w_cursor.lnum;   /* default is current line number */
	ea.cmd = skipwhite(ea.cmd);
	lnum = get_address(&ea.cmd, ea.skip, ea.addr_count == 0);
	if (ea.cmd == NULL)		    /* error detected */
	    goto doend;
	if (lnum == MAXLNUM)
	{
	    if (*ea.cmd == '%')		    /* '%' - all lines */
	    {
		++ea.cmd;
		ea.line1 = 1;
		ea.line2 = curbuf->b_ml.ml_line_count;
		++ea.addr_count;
	    }
					    /* '*' - visual area */
	    else if (*ea.cmd == '*' && vim_strchr(p_cpo, CPO_STAR) == NULL)
	    {
		pos_T	    *fp;

		++ea.cmd;
		if (!ea.skip)
		{
		    fp = getmark('<', FALSE);
		    if (check_mark(fp) == FAIL)
			goto doend;
		    ea.line1 = fp->lnum;
		    fp = getmark('>', FALSE);
		    if (check_mark(fp) == FAIL)
			goto doend;
		    ea.line2 = fp->lnum;
		    ++ea.addr_count;
		}
	    }
	}
	else
	    ea.line2 = lnum;
	ea.addr_count++;

	if (*ea.cmd == ';')
	{
	    if (!ea.skip)
		curwin->w_cursor.lnum = ea.line2;
	}
	else if (*ea.cmd != ',')
	    break;
	++ea.cmd;
    }

    /* One address given: set start and end lines */
    if (ea.addr_count == 1)
    {
	ea.line1 = ea.line2;
	    /* ... but only implicit: really no address given */
	if (lnum == MAXLNUM)
	    ea.addr_count = 0;
    }

    /* Don't leave the cursor on an illegal line (caused by ';') */
    check_cursor_lnum();

/*
 * 4. parse command
 */

    /*
     * Skip ':' and any white space
     */
    ea.cmd = skipwhite(ea.cmd);
    while (*ea.cmd == ':')
	ea.cmd = skipwhite(ea.cmd + 1);

    /*
     * If we got a line, but no command, then go to the line.
     * If we find a '|' or '\n' we set ea.nextcmd.
     */
    if (*ea.cmd == NUL || *ea.cmd == '"' ||
			       (ea.nextcmd = check_nextcmd(ea.cmd)) != NULL)
    {
	/*
	 * strange vi behaviour:
	 * ":3"		jumps to line 3
	 * ":3|..."	prints line 3
	 * ":|"		prints current line
	 */
	if (ea.skip)	    /* skip this if inside :if */
	    goto doend;
	if (*ea.cmd == '|')
	{
	    ea.cmdidx = CMD_print;
	    ea.argt = RANGE+COUNT+TRLBAR;
	    if ((errormsg = invalid_range(&ea)) == NULL)
	    {
		correct_range(&ea);
		ex_print(&ea);
	    }
	}
	else if (ea.addr_count != 0)
	{
	    if (ea.line2 < 0)
		errormsg = invalid_range(&ea);
	    else
	    {
		if (ea.line2 == 0)
		    curwin->w_cursor.lnum = 1;
		else if (ea.line2 > curbuf->b_ml.ml_line_count)
		    curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
		else
		    curwin->w_cursor.lnum = ea.line2;
		beginline(BL_SOL | BL_FIX);
	    }
	}
	goto doend;
    }

    /* Find the command and let "p" point to after it. */
    p = find_command(&ea, NULL);

#ifdef FEAT_USR_CMDS
    if (p == NULL)
    {
	if (!ea.skip)
	    errormsg = (char_u *)_("E464: Ambiguous use of user-defined command");
	goto doend;
    }
    /* Check for wrong commands. */
    if (*p == '!' && ea.cmd[1] == 0151 && ea.cmd[0] == 78)
    {
	errormsg = uc_fun_cmd();
	goto doend;
    }
#endif
    if (ea.cmdidx == CMD_SIZE)
    {
	if (!ea.skip)
	{
	    STRCPY(IObuff, _("E492: Not an editor command"));
	    if (!sourcing)
	    {
		STRCAT(IObuff, ": ");
		STRNCAT(IObuff, *cmdlinep, 40);
	    }
	    errormsg = IObuff;
	}
	goto doend;
    }

    ni = (
#ifdef FEAT_USR_CMDS
	    !USER_CMDIDX(ea.cmdidx) &&
#endif
	    cmdnames[ea.cmdidx].cmd_func == ex_ni);

#ifndef FEAT_EVAL
    /*
     * When the expression evaluation is disabled, recognize the ":if" and
     * ":endif" commands and ignore everything in between it.
     */
    if (ea.cmdidx == CMD_if)
	++if_level;
    if (if_level)
    {
	if (ea.cmdidx == CMD_endif)
	    --if_level;
	goto doend;
    }

#endif

    if (*p == '!' && ea.cmdidx != CMD_substitute)    /* forced commands */
    {
	++p;
	ea.forceit = TRUE;
    }
    else
	ea.forceit = FALSE;

/*
 * 5. parse arguments
 */
#ifdef FEAT_USR_CMDS
    if (!USER_CMDIDX(ea.cmdidx))
#endif
	ea.argt = cmdnames[(int)ea.cmdidx].cmd_argt;

    if (!ea.skip)
    {
#ifdef HAVE_SANDBOX
	if (sandbox != 0 && !(ea.argt & SBOXOK))
	{
	    /* Command not allowed in sandbox. */
	    errormsg = (char_u *)_(e_sandbox);
	    goto doend;
	}
#endif
	if (!curbuf->b_p_ma && (ea.argt & MODIFY))
	{
	    /* Command not allowed in non-'modifiable' buffer */
	    errormsg = (char_u *)_(e_modifiable);
	    goto doend;
	}
#ifdef FEAT_CMDWIN
	if (cmdwin_type != 0 && !(ea.argt & CMDWIN)
# ifdef FEAT_USR_CMDS
		&& !USER_CMDIDX(ea.cmdidx)
# endif
	   )
	{
	    /* Command not allowed in cmdline window. */
	    errormsg = (char_u *)_(e_cmdwin);
	    goto doend;
	}
#endif

	if (!ni && !(ea.argt & RANGE) && ea.addr_count > 0)
	{
	    /* no range allowed */
	    errormsg = (char_u *)_(e_norange);
	    goto doend;
	}
    }

    if (!ni && !(ea.argt & BANG) && ea.forceit)	/* no <!> allowed */
    {
	errormsg = (char_u *)_(e_nobang);
	if (ea.cmdidx == CMD_help)
	    errormsg = (char_u *)_("E478: Don't panic!");
	goto doend;
    }

    /*
     * Don't complain about the range if it is not used
     * (could happen if line_count is accidentally set to 0).
     */
    if (!ea.skip && !ni)
    {
	/*
	 * If the range is backwards, ask for confirmation and, if given, swap
	 * ea.line1 & ea.line2 so it's forwards again.
	 * When global command is busy, don't ask, will fail below.
	 */
	if (!global_busy && ea.line1 > ea.line2)
	{
	    if (sourcing)
	    {
		errormsg = (char_u *)_("E493: Backwards range given");
		goto doend;
	    }
	    else
	    {
		int	msg_silent_save = msg_silent;

		msg_silent = 0;
		if (ask_yesno((char_u *)
			_("Backwards range given, OK to swap"), FALSE) != 'y')
		goto doend;
		msg_silent = msg_silent_save;
	    }
	    lnum = ea.line1;
	    ea.line1 = ea.line2;
	    ea.line2 = lnum;
	}
	if ((errormsg = invalid_range(&ea)) != NULL)
	    goto doend;
    }

    if ((ea.argt & NOTADR) && ea.addr_count == 0) /* default is 1, not cursor */
	ea.line2 = 1;

    correct_range(&ea);

#ifdef FEAT_FOLDING
    if (((ea.argt & WHOLEFOLD) || ea.addr_count >= 2) && !global_busy)
    {
	/* Put the first line at the start of a closed fold, put the last line
	 * at the end of a closed fold. */
	(void)hasFolding(ea.line1, &ea.line1, NULL);
	(void)hasFolding(ea.line2, NULL, &ea.line2);
    }
#endif

#ifdef FEAT_QUICKFIX
    /*
     * For the :make and :grep commands we insert the 'makeprg'/'grepprg'
     * option here, so things like % get expanded.
     */
    if (ea.cmdidx == CMD_make || ea.cmdidx == CMD_grep
						  || ea.cmdidx == CMD_grepadd)
    {
	char_u		*new_cmdline;
	char_u		*program;
	char_u		*pos;
	char_u		*ptr;
	int		len;
	int		i;

	if (ea.cmdidx == CMD_grep || ea.cmdidx == CMD_grepadd)
	{
	    if (*curbuf->b_p_gp == NUL)
		program = p_gp;
	    else
		program = curbuf->b_p_gp;
	}
	else
	{
	    if (*curbuf->b_p_mp == NUL)
		program = p_mp;
	    else
		program = curbuf->b_p_mp;
	}

	p = skipwhite(p);

	if ((pos = (char_u *)strstr((char *)program, "$*")) != NULL)
	{				/* replace $* by given arguments */
	    i = 1;
	    while ((pos = (char_u *)strstr((char *)pos + 2, "$*")) != NULL)
		++i;
	    len = (int)STRLEN(p);
	    new_cmdline = alloc((int)(STRLEN(program) + i * (len - 2) + 1));
	    if (new_cmdline == NULL)
		goto doend;		    /* out of memory */
	    ptr = new_cmdline;
	    while ((pos = (char_u *)strstr((char *)program, "$*")) != NULL)
	    {
		i = (int)(pos - program);
		STRNCPY(ptr, program, i);
		STRCPY(ptr += i, p);
		ptr += len;
		program = pos + 2;
	    }
	    STRCPY(ptr, program);
	}
	else
	{
	    new_cmdline = alloc((int)(STRLEN(program) + STRLEN(p) + 2));
	    if (new_cmdline == NULL)
		goto doend;		    /* out of memory */
	    STRCPY(new_cmdline, program);
	    STRCAT(new_cmdline, " ");
	    STRCAT(new_cmdline, p);
	}
	msg_make(p);
	/* 'ea.cmd' is not set here, because it is not used at CMD_make */
	vim_free(*cmdlinep);
	*cmdlinep = new_cmdline;
	p = new_cmdline;
    }
#endif

    /*
     * Skip to start of argument.
     * Don't do this for the ":!" command, because ":!! -l" needs the space.
     */
    if (ea.cmdidx == CMD_bang)
	ea.arg = p;
    else
	ea.arg = skipwhite(p);

    if (ea.cmdidx == CMD_write || ea.cmdidx == CMD_update)
    {
	if (*ea.arg == '>')			/* append */
	{
	    if (*++ea.arg != '>')		/* typed wrong */
	    {
		errormsg = (char_u *)_("E494: Use w or w>>");
		goto doend;
	    }
	    ea.arg = skipwhite(ea.arg + 1);
	    ea.append = TRUE;
	}
	else if (*ea.arg == '!' && ea.cmdidx == CMD_write)  /* :w !filter */
	{
	    ++ea.arg;
	    ea.usefilter = TRUE;
	}
    }

    if (ea.cmdidx == CMD_read)
    {
	if (ea.forceit)
	{
	    ea.usefilter = TRUE;		/* :r! filter if ea.forceit */
	    ea.forceit = FALSE;
	}
	else if (*ea.arg == '!')		/* :r !filter */
	{
	    ++ea.arg;
	    ea.usefilter = TRUE;
	}
    }

    if (ea.cmdidx == CMD_lshift || ea.cmdidx == CMD_rshift)
    {
	ea.amount = 1;
	while (*ea.arg == *ea.cmd)		/* count number of '>' or '<' */
	{
	    ++ea.arg;
	    ++ea.amount;
	}
	ea.arg = skipwhite(ea.arg);
    }

    /*
     * Check for "++opt=val" argument.
     */
    if (ea.argt & ARGOPT)
	while (ea.arg[0] == '+' && ea.arg[1] == '+')
	    if (getargopt(&ea) == FAIL && !ni)
	    {
		errormsg = (char_u *)_(e_invarg);
		goto doend;
	    }

    /*
     * Check for "+command" argument, before checking for next command.
     * Don't do this for ":read !cmd" and ":write !cmd".
     */
    if ((ea.argt & EDITCMD) && !ea.usefilter)
	ea.do_ecmd_cmd = getargcmd(&ea.arg);

    /*
     * Check for '|' to separate commands and '"' to start comments.
     * Don't do this for ":read !cmd" and ":write !cmd".
     */
    if ((ea.argt & TRLBAR) && !ea.usefilter)
	separate_nextcmd(&ea);

    /*
     * Check for <newline> to end a shell command.
     * Also do this for ":read !cmd" and ":write !cmd".
     */
    else if (ea.cmdidx == CMD_bang || ea.usefilter)
    {
	for (p = ea.arg; *p; ++p)
	{
	    /* Remove one backslash before a newline, so that it's possible to
	     * pass a newline to the shell and also a newline that is preceded
	     * with a backslash.  This makes it impossible to end a shell
	     * command in a backslash, but that doesn't appear useful.
	     * Halving the number of backslashes is incompatible with previous
	     * versions. */
	    if (*p == '\\' && p[1] == '\n')
		mch_memmove(p, p + 1, STRLEN(p));
	    else if (*p == '\n')
	    {
		ea.nextcmd = p + 1;
		*p = NUL;
		break;
	    }
	}
    }

    if ((ea.argt & DFLALL) && ea.addr_count == 0)
    {
	ea.line1 = 1;
	ea.line2 = curbuf->b_ml.ml_line_count;
    }

    /* accept numbered register only when no count allowed (:put) */
    if (       (ea.argt & REGSTR)
	    && *ea.arg != NUL
#ifdef FEAT_USR_CMDS
	    && valid_yank_reg(*ea.arg, (ea.cmdidx != CMD_put
						   && USER_CMDIDX(ea.cmdidx)))
	    /* Do not allow register = for user commands */
	    && (!USER_CMDIDX(ea.cmdidx) || *ea.arg != '=')
#else
	    && valid_yank_reg(*ea.arg, ea.cmdidx != CMD_put)
#endif
	    && !((ea.argt & COUNT) && isdigit(*ea.arg)))
    {
	ea.regname = *ea.arg++;
#ifdef FEAT_EVAL
	/* for '=' register: accept the rest of the line as an expression */
	if (ea.arg[-1] == '=' && ea.arg[0] != NUL)
	{
	    set_expr_line(vim_strsave(ea.arg));
	    ea.arg += STRLEN(ea.arg);
	}
#endif
	ea.arg = skipwhite(ea.arg);
    }

    /*
     * Check for a count.  When accepting a BUFNAME, don't use "123foo" as a
     * count, it's a buffer name.
     */
    if ((ea.argt & COUNT) && isdigit(*ea.arg)
	    && (!(ea.argt & BUFNAME) || *(p = skipdigits(ea.arg)) == NUL
							  || vim_iswhite(*p)))
    {
	n = getdigits(&ea.arg);
	ea.arg = skipwhite(ea.arg);
	if (n <= 0 && !ni)
	{
	    errormsg = (char_u *)_(e_zerocount);
	    goto doend;
	}
	if (ea.argt & NOTADR)	/* e.g. :buffer 2, :sleep 3 */
	{
	    ea.line2 = n;
	    if (ea.addr_count == 0)
		ea.addr_count = 1;
	}
	else
	{
	    ea.line1 = ea.line2;
	    ea.line2 += n - 1;
	    ++ea.addr_count;
	    /*
	     * Be vi compatible: no error message for out of range.
	     */
	    if (ea.line2 > curbuf->b_ml.ml_line_count)
		ea.line2 = curbuf->b_ml.ml_line_count;
	}
    }
						/* no arguments allowed */
    if (!ni && !(ea.argt & EXTRA) && *ea.arg != NUL &&
				 vim_strchr((char_u *)"|\"", *ea.arg) == NULL)
    {
	errormsg = (char_u *)_(e_trailing);
	goto doend;
    }

    if (!ni && (ea.argt & NEEDARG) && *ea.arg == NUL)
    {
	errormsg = (char_u *)_(e_argreq);
	goto doend;
    }

#ifdef FEAT_EVAL
    /*
     * Skip the command when it's not going to be executed.
     * The commands like :if, :endif, etc. always need to be executed.
     * Also make an exception for commands that handle a trailing command
     * themselves.
     */
    if (ea.skip)
    {
	switch (ea.cmdidx)
	{
	    /* commands that need evaluation */
	    case CMD_while:
	    case CMD_endwhile:
	    case CMD_if:
	    case CMD_elseif:
	    case CMD_else:
	    case CMD_endif:
	    case CMD_try:
	    case CMD_catch:
	    case CMD_finally:
	    case CMD_endtry:
	    case CMD_function:
				break;

	    /* Commands that handle '|' themselves.  Check: A command should
	     * either have the TRLBAR flag, appear in this list or appear in
	     * the list at ":help :bar". */
	    case CMD_aboveleft:
	    case CMD_and:
	    case CMD_belowright:
	    case CMD_botright:
	    case CMD_browse:
	    case CMD_call:
	    case CMD_confirm:
	    case CMD_delfunction:
	    case CMD_djump:
	    case CMD_dlist:
	    case CMD_dsearch:
	    case CMD_dsplit:
	    case CMD_echo:
	    case CMD_echoerr:
	    case CMD_echomsg:
	    case CMD_echon:
	    case CMD_execute:
	    case CMD_help:
	    case CMD_hide:
	    case CMD_ijump:
	    case CMD_ilist:
	    case CMD_isearch:
	    case CMD_isplit:
	    case CMD_leftabove:
	    case CMD_let:
	    case CMD_match:
	    case CMD_psearch:
	    case CMD_return:
	    case CMD_throw:
	    case CMD_rightbelow:
	    case CMD_silent:
	    case CMD_smagic:
	    case CMD_snomagic:
	    case CMD_substitute:
	    case CMD_syntax:
	    case CMD_tilde:
	    case CMD_topleft:
	    case CMD_unlet:
	    case CMD_verbose:
	    case CMD_vertical:
				break;

	    default:		goto doend;
	}
    }
#endif

    if (ea.argt & XFILE)
    {
	if (expand_filename(&ea, cmdlinep, &errormsg) == FAIL)
	    goto doend;
    }

#ifdef FEAT_LISTCMDS
    /*
     * Accept buffer name.  Cannot be used at the same time with a buffer
     * number.  Don't do this for a user command.
     */
    if ((ea.argt & BUFNAME) && *ea.arg != NUL && ea.addr_count == 0
# ifdef FEAT_USR_CMDS
	    && !USER_CMDIDX(ea.cmdidx)
# endif
	    )
    {
	/*
	 * :bdelete, :bwipeout and :bunload take several arguments, separated
	 * by spaces: find next space (skipping over escaped characters).
	 * The others take one argument: ignore trailing spaces.
	 */
	if (ea.cmdidx == CMD_bdelete || ea.cmdidx == CMD_bwipeout
						  || ea.cmdidx == CMD_bunload)
	    p = skiptowhite_esc(ea.arg);
	else
	{
	    p = ea.arg + STRLEN(ea.arg);
	    while (p > ea.arg && vim_iswhite(p[-1]))
		--p;
	}
	ea.line2 = buflist_findpat(ea.arg, p, (ea.argt & BUFUNL) != 0, FALSE);
	if (ea.line2 < 0)	    /* failed */
	    goto doend;
	ea.addr_count = 1;
	ea.arg = skipwhite(p);
    }
#endif

/*
 * 6. switch on command name
 *
 * The "ea" structure holds the arguments that can be used.
 */
    ea.cmdlinep = cmdlinep;
    ea.getline = getline;
    ea.cookie = cookie;
#ifdef FEAT_EVAL
    ea.cstack = cstack;
#endif

#ifdef FEAT_USR_CMDS
    if (USER_CMDIDX(ea.cmdidx))
    {
	/*
	 * Execute a user-defined command.
	 */
	do_ucmd(&ea);
    }
    else
#endif
    {
	/*
	 * Call the function to execute the command.
	 */
	ea.errmsg = NULL;
	(cmdnames[ea.cmdidx].cmd_func)(&ea);
	if (ea.errmsg != NULL)
	    errormsg = (char_u *)_(ea.errmsg);
    }

#ifdef FEAT_EVAL
    /*
     * If the command just executed called do_cmdline(), any throw or ":return"
     * or ":finish" encountered there must also check the cstack of the still
     * active do_cmdline() that called this do_one_cmd().  Rethrow an uncaught
     * exception, or reanimate a returned function or finished script file and
     * return or finish it again.
     */
    if (need_rethrow)
	do_throw(cstack);
    else if (check_cstack)
    {
	if (getline == getsourceline && source_finished(cookie))
	    do_finish(&ea, TRUE);
	else if (getline == get_func_line && current_func_returned())
	    do_return(&ea, TRUE, FALSE, NULL);
    }
    need_rethrow = check_cstack = FALSE;
#endif

doend:
    if (curwin->w_cursor.lnum == 0)	/* can happen with zero line number */
	curwin->w_cursor.lnum = 1;

    if (errormsg != NULL && *errormsg != NUL && !did_emsg)
    {
	if (sourcing)
	{
	    if (errormsg != IObuff)
	    {
		STRCPY(IObuff, errormsg);
		errormsg = IObuff;
	    }
	    STRCAT(errormsg, ": ");
	    STRNCAT(errormsg, *cmdlinep, IOSIZE - STRLEN(IObuff));
	}
	emsg(errormsg);
    }
#ifdef FEAT_EVAL
    do_errthrow(cstack,
	    (ea.cmdidx != CMD_SIZE
# ifdef FEAT_USR_CMDS
	     && !USER_CMDIDX(ea.cmdidx)
# endif
	    ) ? cmdnames[(int)ea.cmdidx].cmd_name : (char_u *)NULL);
#endif

    if (verbose_save >= 0)
	p_verbose = verbose_save;

    cmdmod = save_cmdmod;

    if (did_silent > 0)
    {
	/* messages could be enabled for a serious error, need to check if the
	 * counters don't become negative */
	msg_silent -= did_silent;
	if (msg_silent < 0)
	    msg_silent = 0;
	emsg_silent -= did_esilent;
	if (emsg_silent < 0)
	    emsg_silent = 0;
	/* Restore msg_scroll, it's set by file I/O commands, even when no
	 * message is actually displayed. */
	msg_scroll = save_msg_scroll;
    }

    if (ea.nextcmd && *ea.nextcmd == NUL)	/* not really a next command */
	ea.nextcmd = NULL;

#ifdef FEAT_EVAL
    --ex_nesting_level;
#endif

    return ea.nextcmd;
}
#if (_MSC_VER == 1200)
#pragma optimize( "", on )
#endif

/*
 * Check for a command modifier command with optional tail.
 * If there is a match advance "pp" to the argument and return TRUE.
 */
    static int
checkforcmd(pp, cmd, len)
    char_u	**pp;		/* start of command line */
    char	*cmd;		/* name of command */
    int		len;		/* required length */
{
    int		i;

    for (i = 0; cmd[i] != NUL; ++i)
	if (cmd[i] != (*pp)[i])
	    break;
    if (i >= len && !isalpha((*pp)[i]))
    {
	*pp = skipwhite(*pp + i);
	return TRUE;
    }
    return FALSE;
}

/*
 * Find an Ex command by its name, either built-in or user.
 * Name can be found at eap->cmd.
 * Returns pointer to char after the command name.
 * Returns NULL for an ambiguous user command.
 */
/*ARGSUSED*/
    static char_u *
find_command(eap, full)
    exarg_T	*eap;
    int		*full;
{
    int		len;
    char_u	*p;

    /*
     * Isolate the command and search for it in the command table.
     * Exeptions:
     * - the 'k' command can directly be followed by any character.
     * - the 's' command can be followed directly by 'c', 'g', 'i', 'I' or 'r'
     *	    but :sre[wind] is another command, as are :scrip[tnames],
     *	    :scs[cope], :sim[alt], :sig[ns] and :sil[ent].
     */
    p = eap->cmd;
    if (*p == 'k')
    {
	eap->cmdidx = CMD_k;
	++p;
    }
    else if (p[0] == 's'
	    && ((p[1] == 'c' && p[2] != 's' && p[2] != 'r' && p[3] != 'i' && p[4] != 'p')
		|| p[1] == 'g'
		|| (p[1] == 'i' && p[2] != 'm' && p[2] != 'l' && p[2] != 'g')
		|| p[1] == 'I'
		|| (p[1] == 'r' && p[2] != 'e')))
    {
	eap->cmdidx = CMD_substitute;
	++p;
    }
    else
    {
	while (ASCII_ISALPHA(*p))
	    ++p;
	/* check for non-alpha command */
	if (p == eap->cmd && vim_strchr((char_u *)"@*!=><&~#", *p) != NULL)
	    ++p;
	len = (int)(p - eap->cmd);

	if (ASCII_ISLOWER(*eap->cmd))
	    eap->cmdidx = cmdidxs[CharOrdLow(*eap->cmd)];
	else
	    eap->cmdidx = cmdidxs[26];

	for ( ; (int)eap->cmdidx < (int)CMD_SIZE;
			       eap->cmdidx = (cmdidx_T)((int)eap->cmdidx + 1))
	    if (STRNCMP(cmdnames[(int)eap->cmdidx].cmd_name, (char *)eap->cmd,
							    (size_t)len) == 0)
	    {
#ifdef FEAT_EVAL
		if (full != NULL
			   && cmdnames[(int)eap->cmdidx].cmd_name[len] == NUL)
		    *full = TRUE;
#endif
		break;
	    }

#ifdef FEAT_USR_CMDS
	/* Look for a user defined command as a last resort */
	if (eap->cmdidx == CMD_SIZE && *eap->cmd >= 'A' && *eap->cmd <= 'Z')
	{
	    ucmd_T	*cmd;
	    int		j, k, matchlen = 0;
	    int		found = FALSE, possible = FALSE;
	    char_u	*cp, *np;	/* Point into typed cmd and test name */
	    garray_T	*gap;
	    int		amb_local = FALSE; /* Found ambiguous buffer-local
					      command, only full match global
					      is accepted. */

	    /* User defined commands may contain numbers */
	    while (ASCII_ISALNUM(*p))
		++p;
	    len = (int)(p - eap->cmd);

	    /*
	     * Look for buffer-local user commands first, then global ones.
	     */
	    gap = &curbuf->b_ucmds;
	    for (;;)
	    {
		cmd = USER_CMD_GA(gap, 0);

		for (j = 0; j < gap->ga_len; ++j, ++cmd)
		{
		    cp = eap->cmd;
		    np = cmd->uc_name;
		    k = 0;
		    while (k < len && *np != NUL && *cp++ == *np++)
			k++;
		    if (k == len || (*np == NUL && isdigit(eap->cmd[k])))
		    {
			/* If finding a second match, the command is
			 * ambiguous.  But not if a buffer-local command
			 * wasn't a full match and a global command is a full
			 * match. */
			if (k == len && found && *np != NUL)
			{
			    if (gap == &ucmds)
				return NULL;
			    amb_local = TRUE;
			}

			if (!found || (k == len && *np == NUL))
			{
			    /* If we matched up to a digit, then there could
			     * be another command including the digit that we
			     * should use instead.
			     */
			    if (k == len)
				found = TRUE;
			    else
				possible = TRUE;

			    if (gap == &ucmds)
				eap->cmdidx = CMD_USER;
			    else
				eap->cmdidx = CMD_USER_BUF;
			    eap->argt = cmd->uc_argt;
			    eap->useridx = j;

			    /* Do not search for further abbreviations
			     * if this is an exact match. */
			    matchlen = k;
			    if (k == len && *np == NUL)
			    {
				if (full != NULL)
				    *full = TRUE;
				amb_local = FALSE;
				break;
			    }
			}
		    }
		}

		/* Stop if we found a full match or searched all. */
		if (j < gap->ga_len || gap == &ucmds)
		    break;
		gap = &ucmds;
	    }

	    /* Only found ambiguous matches. */
	    if (amb_local)
		return NULL;

	    /* The match we found may be followed immediately by a
	     * number.  Move *p back to point to it.
	     */
	    if (found || possible)
		p += matchlen - len;
	}
#endif

	if (len == 0)
	    eap->cmdidx = CMD_SIZE;
    }

    return p;
}

#if defined(FEAT_EVAL) || defined(PROTO)
/*
 * Return > 0 if an Ex command "name" exists.
 * Return 2 if there is an exact match.
 * Return 3 if there is an ambiguous match.
 */
    int
cmd_exists(name)
    char_u	*name;
{
    exarg_T	ea;
    int		full = FALSE;

    ea.cmd = name;
    ea.cmdidx = (cmdidx_T)0;
    if (find_command(&ea, &full) == NULL)
	return 3;
    return (ea.cmdidx == CMD_SIZE ? 0 : (full ? 2 : 1));
}
#endif

/*
 * This is all pretty much copied from do_one_cmd(), with all the extra stuff
 * we don't need/want deleted.	Maybe this could be done better if we didn't
 * repeat all this stuff.  The only problem is that they may not stay
 * perfectly compatible with each other, but then the command line syntax
 * probably won't change that much -- webb.
 */
    char_u *
set_one_cmd_context(xp, buff)
    expand_T	*xp;
    char_u	*buff;	    /* buffer for command string */
{
    char_u		*p;
    char_u		*cmd, *arg;
    int			i = 0;
    cmdidx_T		cmdidx;
    long_u		argt = 0;
#if defined(FEAT_USR_CMDS) && defined(FEAT_CMDL_COMPL)
    int			compl = EXPAND_NOTHING;
#endif
#ifdef FEAT_CMDL_COMPL
    int			delim;
#endif
    int			forceit = FALSE;
    int			usefilter = FALSE;  /* filter instead of file name */

    xp->xp_pattern = buff;
    xp->xp_context = EXPAND_COMMANDS;	/* Default until we get past command */
    xp->xp_backslash = XP_BS_NONE;
    xp->xp_arg = NULL;

/*
 * 2. skip comment lines and leading space, colons or bars
 */
    for (cmd = buff; vim_strchr((char_u *)" \t:|", *cmd) != NULL; cmd++)
	;
    xp->xp_pattern = cmd;

    if (*cmd == NUL)
	return NULL;
    if (*cmd == '"')	    /* ignore comment lines */
    {
	xp->xp_context = EXPAND_NOTHING;
	return NULL;
    }

/*
 * 3. parse a range specifier of the form: addr [,addr] [;addr] ..
 */
    cmd = skip_range(cmd, &xp->xp_context);

/*
 * 4. parse command
 */

    cmd = skipwhite(cmd);
    xp->xp_pattern = cmd;
    if (*cmd == NUL)
	return NULL;
    if (*cmd == '"')
    {
	xp->xp_context = EXPAND_NOTHING;
	return NULL;
    }

    if (*cmd == '|' || *cmd == '\n')
	return cmd + 1;			/* There's another command */

    /*
     * Isolate the command and search for it in the command table.
     * Exceptions:
     * - the 'k' command can directly be followed by any character.
     * - the 's' command can be followed directly by 'c', 'g', 'i', 'I' or 'r'
     */
    if (*cmd == 'k')
    {
	cmdidx = CMD_k;
	p = cmd + 1;
    }
    else
    {
	p = cmd;
	while (ASCII_ISALPHA(*p) || *p == '*')    /* Allow * wild card */
	    ++p;
	/* check for non-alpha command */
	if (p == cmd && vim_strchr((char_u *)"@*!=><&~#", *p) != NULL)
	    ++p;
	i = (int)(p - cmd);

	if (i == 0)
	{
	    xp->xp_context = EXPAND_UNSUCCESSFUL;
	    return NULL;
	}
	for (cmdidx = (cmdidx_T)0; (int)cmdidx < (int)CMD_SIZE;
					 cmdidx = (cmdidx_T)((int)cmdidx + 1))
	    if (STRNCMP(cmdnames[(int)cmdidx].cmd_name, cmd, (size_t)i) == 0)
		break;
#ifdef FEAT_USR_CMDS

	if (cmd[0] >= 'A' && cmd[0] <= 'Z')
	{
	    while (ASCII_ISALNUM(*p) || *p == '*')	/* Allow * wild card */
		++p;
	    i = (int)(p - cmd);
	}
#endif
    }

    /*
     * If the cursor is touching the command, and it ends in an alpha-numeric
     * character, complete the command name.
     */
    if (*p == NUL && ASCII_ISALNUM(p[-1]))
	return NULL;

    if (cmdidx == CMD_SIZE)
    {
	if (*cmd == 's' && vim_strchr((char_u *)"cgriI", cmd[1]) != NULL)
	{
	    cmdidx = CMD_substitute;
	    p = cmd + 1;
	}
#ifdef FEAT_USR_CMDS
	else if (cmd[0] >= 'A' && cmd[0] <= 'Z')
	{
	    /* Look for a user defined command as a last resort */
	    ucmd_T	*uc;
	    int		j, k, matchlen = 0;
	    int		found = FALSE, possible = FALSE;
	    char_u	*cp, *np;	/* Point into typed cmd and test name */
	    garray_T	*gap;

	    gap = &curbuf->b_ucmds;
	    for (;;)
	    {
		uc = USER_CMD_GA(gap, 0);
		for (j = 0; j < gap->ga_len; ++j, ++uc)
		{
		    cp = cmd;
		    np = uc->uc_name;
		    k = 0;
		    while (k < i && *np != NUL && *cp++ == *np++)
			k++;
		    if (k == i || (*np == NUL && isdigit(cmd[k])))
		    {
			if (k == i && found)
			{
			    /* Ambiguous abbreviation */
			    xp->xp_context = EXPAND_UNSUCCESSFUL;
			    return NULL;
			}
			if (!found)
			{
			    /* If we matched up to a digit, then there could
			     * be another command including the digit that we
			     * should use instead.
			     */
			    if (k == i)
				found = TRUE;
			    else
				possible = TRUE;

			    if (gap == &ucmds)
				cmdidx = CMD_USER;
			    else
				cmdidx = CMD_USER_BUF;
			    argt = uc->uc_argt;
#ifdef FEAT_CMDL_COMPL
			    compl = uc->uc_compl;
			    xp->xp_arg = uc->uc_compl_arg;
#endif
			    /* Do not search for further abbreviations
			     * if this is an exact match
			     */
			    matchlen = k;
			    if (k == i && *np == NUL)
				break;
			}
		    }
		}
		if (gap == &ucmds || j < gap->ga_len)
		    break;
		gap = &ucmds;
	    }

	    /* The match we found may be followed immediately by a
	     * number.  Move *p back to point to it.
	     */
	    if (found || possible)
		p += matchlen - i;
	}
#endif
    }
    if (cmdidx == CMD_SIZE)
    {
	/* Not still touching the command and it was an illegal one */
	xp->xp_context = EXPAND_UNSUCCESSFUL;
	return NULL;
    }

    xp->xp_context = EXPAND_NOTHING; /* Default now that we're past command */

    if (*p == '!')		    /* forced commands */
    {
	forceit = TRUE;
	++p;
    }

/*
 * 5. parse arguments
 */
#ifdef FEAT_USR_CMDS
    if (!USER_CMDIDX(cmdidx))
#endif
	argt = cmdnames[(int)cmdidx].cmd_argt;

    arg = skipwhite(p);

    if (cmdidx == CMD_write || cmdidx == CMD_update)
    {
	if (*arg == '>')			/* append */
	{
	    if (*++arg == '>')
		++arg;
	    arg = skipwhite(arg);
	}
	else if (*arg == '!' && cmdidx == CMD_write)	/* :w !filter */
	{
	    ++arg;
	    usefilter = TRUE;
	}
    }

    if (cmdidx == CMD_read)
    {
	usefilter = forceit;			/* :r! filter if forced */
	if (*arg == '!')			/* :r !filter */
	{
	    ++arg;
	    usefilter = TRUE;
	}
    }

    if (cmdidx == CMD_lshift || cmdidx == CMD_rshift)
    {
	while (*arg == *cmd)	    /* allow any number of '>' or '<' */
	    ++arg;
	arg = skipwhite(arg);
    }

    /* Does command allow "+command"? */
    if ((argt & EDITCMD) && !usefilter && *arg == '+')
    {
	/* Check if we're in the +command */
	p = arg + 1;
	arg = skip_cmd_arg(arg, FALSE);

	/* Still touching the command after '+'? */
	if (*arg == NUL)
	    return p;

	/* Skip space(s) after +command to get to the real argument */
	arg = skipwhite(arg);
    }

    /*
     * Check for '|' to separate commands and '"' to start comments.
     * Don't do this for ":read !cmd" and ":write !cmd".
     */
    if ((argt & TRLBAR) && !usefilter)
    {
	p = arg;
	/* ":redir @" is not the start of a comment */
	if (cmdidx == CMD_redir && p[0] == '@' && p[1] == '"')
	    p += 2;
	while (*p)
	{
	    if (*p == Ctrl_V)
	    {
		if (p[1] != NUL)
		    ++p;
	    }
	    else if ( (*p == '"' && !(argt & NOTRLCOM))
		    || *p == '|' || *p == '\n')
	    {
		if (*(p - 1) != '\\')
		{
		    if (*p == '|' || *p == '\n')
			return p + 1;
		    return NULL;    /* It's a comment */
		}
	    }
#ifdef FEAT_MBYTE
	    if (has_mbyte)
		p += (*mb_ptr2len_check)(p);
	    else
#endif
		++p;
	}
    }

						/* no arguments allowed */
    if (!(argt & EXTRA) && *arg != NUL &&
				    vim_strchr((char_u *)"|\"", *arg) == NULL)
	return NULL;

    /* Find start of last argument (argument just before cursor): */
    p = buff + STRLEN(buff);
    while (p != arg && *p != ' ' && *p != TAB)
	p--;
    if (*p == ' ' || *p == TAB)
	p++;
    xp->xp_pattern = p;

    if (argt & XFILE)
    {
	int in_quote = FALSE;
	char_u *bow = NULL;	/* Beginning of word */

	/*
	 * Allow spaces within back-quotes to count as part of the argument
	 * being expanded.
	 */
	xp->xp_pattern = skipwhite(arg);
	for (p = xp->xp_pattern; *p; )
	{
	    if (*p == '\\' && p[1])
		++p;
#ifdef SPACE_IN_FILENAME
	    else if (vim_iswhite(*p) && (!(argt & NOSPC) || usefilter))
#else
	    else if (vim_iswhite(*p))
#endif
	    {
		p = skipwhite(p);
		if (in_quote)
		    bow = p;
		else
		    xp->xp_pattern = p;
		--p;
	    }
	    else if (*p == '`')
	    {
		if (!in_quote)
		{
		    xp->xp_pattern = p;
		    bow = p + 1;
		}
		in_quote = !in_quote;
	    }
#ifdef FEAT_MBYTE
	    if (has_mbyte)
		p += (*mb_ptr2len_check)(p);
	    else
#endif
		++p;
	}

	/*
	 * If we are still inside the quotes, and we passed a space, just
	 * expand from there.
	 */
	if (bow != NULL && in_quote)
	    xp->xp_pattern = bow;
	xp->xp_context = EXPAND_FILES;

	/* Check for environment variable */
	if (*xp->xp_pattern == '$'
#if defined(MSDOS) || defined(MSWIN) || defined(OS2)
		|| *xp->xp_pattern == '%'
#endif
		)
	{
	    for (p = xp->xp_pattern + 1; *p != NUL; ++p)
		if (!vim_isIDc(*p))
		    break;
	    if (*p == NUL)
	    {
		xp->xp_context = EXPAND_ENV_VARS;
		++xp->xp_pattern;
	    }
	}
    }

/*
 * 6. switch on command name
 */
    switch (cmdidx)
    {
	case CMD_cd:
	case CMD_chdir:
	case CMD_lcd:
	case CMD_lchdir:
	    if (xp->xp_context == EXPAND_FILES)
		xp->xp_context = EXPAND_DIRECTORIES;
	    break;
	case CMD_help:
	    xp->xp_context = EXPAND_HELP;
	    xp->xp_pattern = arg;
	    break;

	/* Command modifiers: return the argument. */
	case CMD_aboveleft:
	case CMD_belowright:
	case CMD_botright:
	case CMD_browse:
	case CMD_confirm:
	case CMD_folddoclosed:
	case CMD_folddoopen:
	case CMD_hide:
	case CMD_leftabove:
	case CMD_rightbelow:
	case CMD_silent:
	case CMD_topleft:
	case CMD_verbose:
	case CMD_vertical:
	    return arg;

#ifdef FEAT_SEARCH_EXTRA
	case CMD_match:
	    if (*arg == NUL || !ends_excmd(*arg))
	    {
		/* Dummy call to clear variables. */
		set_context_in_highlight_cmd(xp, (char_u *)"link n");
		xp->xp_context = EXPAND_HIGHLIGHT;
		xp->xp_pattern = arg;
		arg = skipwhite(skiptowhite(arg));
		if (*arg != NUL)
		{
		    xp->xp_context = EXPAND_NOTHING;
		    arg = skip_regexp(arg + 1, *arg, p_magic, NULL);
		}
	    }
	    return find_nextcmd(arg);
#endif

#ifdef FEAT_CMDL_COMPL
/*
 * All completion for the +cmdline_compl feature goes here.
 */

# ifdef FEAT_USR_CMDS
	case CMD_command:
	    /* Check for attributes */
	    while (*arg == '-')
	    {
		arg++;	    /* Skip "-" */
		p = skiptowhite(arg);
		if (*p == NUL)
		{
		    /* Cursor is still in the attribute */
		    p = vim_strchr(arg, '=');
		    if (p == NULL)
		    {
			/* No "=", so complete attribute names */
			xp->xp_context = EXPAND_USER_CMD_FLAGS;
			xp->xp_pattern = arg;
			return NULL;
		    }

		    /* For the -complete and -nargs attributes, we complete
		     * their arguments as well.
		     */
		    if (STRNICMP(arg, "complete", p - arg) == 0)
		    {
			xp->xp_context = EXPAND_USER_COMPLETE;
			xp->xp_pattern = p + 1;
			return NULL;
		    }
		    else if (STRNICMP(arg, "nargs", p - arg) == 0)
		    {
			xp->xp_context = EXPAND_USER_NARGS;
			xp->xp_pattern = p + 1;
			return NULL;
		    }
		    return NULL;
		}
		arg = skipwhite(p);
	    }

	    /* After the attributes comes the new command name */
	    p = skiptowhite(arg);
	    if (*p == NUL)
	    {
		xp->xp_context = EXPAND_USER_COMMANDS;
		xp->xp_pattern = arg;
		break;
	    }

	    /* And finally comes a normal command */
	    return skipwhite(p);

	case CMD_delcommand:
	    xp->xp_context = EXPAND_USER_COMMANDS;
	    xp->xp_pattern = arg;
	    break;
# endif

	case CMD_global:
	case CMD_vglobal:
	    delim = *arg;	    /* get the delimiter */
	    if (delim)
		++arg;		    /* skip delimiter if there is one */

	    while (arg[0] != NUL && arg[0] != delim)
	    {
		if (arg[0] == '\\' && arg[1] != NUL)
		    ++arg;
		++arg;
	    }
	    if (arg[0] != NUL)
		return arg + 1;
	    break;
	case CMD_and:
	case CMD_substitute:
	    delim = *arg;
	    if (delim)
	    {
		/* skip "from" part */
		++arg;
		arg = skip_regexp(arg, delim, p_magic, NULL);
	    }
	    /* skip "to" part */
	    while (arg[0] != NUL && arg[0] != delim)
	    {
		if (arg[0] == '\\' && arg[1] != NUL)
		    ++arg;
		++arg;
	    }
	    if (arg[0] != NUL)	/* skip delimiter */
		++arg;
	    while (arg[0] && vim_strchr((char_u *)"|\"#", arg[0]) == NULL)
		++arg;
	    if (arg[0] != NUL)
		return arg;
	    break;
	case CMD_isearch:
	case CMD_dsearch:
	case CMD_ilist:
	case CMD_dlist:
	case CMD_ijump:
	case CMD_psearch:
	case CMD_djump:
	case CMD_isplit:
	case CMD_dsplit:
	    arg = skipwhite(skipdigits(arg));	    /* skip count */
	    if (*arg == '/')	/* Match regexp, not just whole words */
	    {
		for (++arg; *arg && *arg != '/'; arg++)
		    if (*arg == '\\' && arg[1] != NUL)
			arg++;
		if (*arg)
		{
		    arg = skipwhite(arg + 1);

		    /* Check for trailing illegal characters */
		    if (*arg && vim_strchr((char_u *)"|\"\n", *arg) == NULL)
			xp->xp_context = EXPAND_NOTHING;
		    else
			return arg;
		}
	    }
	    break;
#ifdef FEAT_AUTOCMD
	case CMD_autocmd:
	    return set_context_in_autocmd(xp, arg, FALSE);

	case CMD_doautocmd:
	    return set_context_in_autocmd(xp, arg, TRUE);
#endif
	case CMD_set:
	    set_context_in_set_cmd(xp, arg, 0);
	    break;
	case CMD_setglobal:
	    set_context_in_set_cmd(xp, arg, OPT_GLOBAL);
	    break;
	case CMD_setlocal:
	    set_context_in_set_cmd(xp, arg, OPT_LOCAL);
	    break;
	case CMD_tag:
	case CMD_stag:
	case CMD_ptag:
	case CMD_tselect:
	case CMD_stselect:
	case CMD_ptselect:
	case CMD_tjump:
	case CMD_stjump:
	case CMD_ptjump:
	    xp->xp_context = EXPAND_TAGS;
	    xp->xp_pattern = arg;
	    break;
	case CMD_augroup:
	    xp->xp_context = EXPAND_AUGROUP;
	    xp->xp_pattern = arg;
	    break;
#ifdef FEAT_SYN_HL
	case CMD_syntax:
	    set_context_in_syntax_cmd(xp, arg);
	    break;
#endif
#ifdef FEAT_EVAL
	case CMD_let:
	case CMD_if:
	case CMD_elseif:
	case CMD_while:
	case CMD_echo:
	case CMD_echon:
	case CMD_execute:
	case CMD_echomsg:
	case CMD_echoerr:
	case CMD_call:
	case CMD_return:
	    set_context_for_expression(xp, arg, cmdidx);
	    break;

	case CMD_unlet:
	    while ((xp->xp_pattern = vim_strchr(arg, ' ')) != NULL)
		arg = xp->xp_pattern + 1;
	    xp->xp_context = EXPAND_USER_VARS;
	    xp->xp_pattern = arg;
	    break;

	case CMD_function:
	case CMD_delfunction:
	    xp->xp_context = EXPAND_USER_FUNC;
	    xp->xp_pattern = arg;
	    break;

	case CMD_echohl:
	    xp->xp_context = EXPAND_HIGHLIGHT;
	    xp->xp_pattern = arg;
	    break;
#endif
	case CMD_highlight:
	    set_context_in_highlight_cmd(xp, arg);
	    break;
#ifdef FEAT_LISTCMDS
	case CMD_bdelete:
	case CMD_bwipeout:
	case CMD_bunload:
	    while ((xp->xp_pattern = vim_strchr(arg, ' ')) != NULL)
		arg = xp->xp_pattern + 1;
	    /*FALLTHROUGH*/
	case CMD_buffer:
	case CMD_sbuffer:
	case CMD_checktime:
	    xp->xp_context = EXPAND_BUFFERS;
	    xp->xp_pattern = arg;
	    break;
#endif
#ifdef FEAT_USR_CMDS
	case CMD_USER:
	case CMD_USER_BUF:
	    if (compl != EXPAND_NOTHING)
	    {
		/* XFILE: file names are handled above */
		if (!(argt & XFILE))
		{
# ifdef FEAT_MENU
		    if (compl == EXPAND_MENUS)
			return set_context_in_menu_cmd(xp, cmd, arg, forceit);
# endif
		    if (compl == EXPAND_COMMANDS)
			return arg;
		    if (compl == EXPAND_MAPPINGS)
			return set_context_in_map_cmd(xp, (char_u *)"map",
					 arg, forceit, FALSE, FALSE, CMD_map);
		    while ((xp->xp_pattern = vim_strchr(arg, ' ')) != NULL)
			arg = xp->xp_pattern + 1;
		    xp->xp_pattern = arg;
		}
		xp->xp_context = compl;
	    }
	    break;
#endif
	case CMD_map:	    case CMD_noremap:
	case CMD_nmap:	    case CMD_nnoremap:
	case CMD_vmap:	    case CMD_vnoremap:
	case CMD_omap:	    case CMD_onoremap:
	case CMD_imap:	    case CMD_inoremap:
	case CMD_cmap:	    case CMD_cnoremap:
	    return set_context_in_map_cmd(xp, cmd, arg, forceit,
							FALSE, FALSE, cmdidx);
	case CMD_unmap:
	case CMD_nunmap:
	case CMD_vunmap:
	case CMD_ounmap:
	case CMD_iunmap:
	case CMD_cunmap:
	    return set_context_in_map_cmd(xp, cmd, arg, forceit,
							 FALSE, TRUE, cmdidx);
	case CMD_abbreviate:	case CMD_noreabbrev:
	case CMD_cabbrev:	case CMD_cnoreabbrev:
	case CMD_iabbrev:	case CMD_inoreabbrev:
	    return set_context_in_map_cmd(xp, cmd, arg, forceit,
							 TRUE, FALSE, cmdidx);
	case CMD_unabbreviate:
	case CMD_cunabbrev:
	case CMD_iunabbrev:
	    return set_context_in_map_cmd(xp, cmd, arg, forceit,
							  TRUE, TRUE, cmdidx);
#ifdef FEAT_MENU
	case CMD_menu:	    case CMD_noremenu:	    case CMD_unmenu:
	case CMD_amenu:	    case CMD_anoremenu:	    case CMD_aunmenu:
	case CMD_nmenu:	    case CMD_nnoremenu:	    case CMD_nunmenu:
	case CMD_vmenu:	    case CMD_vnoremenu:	    case CMD_vunmenu:
	case CMD_omenu:	    case CMD_onoremenu:	    case CMD_ounmenu:
	case CMD_imenu:	    case CMD_inoremenu:	    case CMD_iunmenu:
	case CMD_cmenu:	    case CMD_cnoremenu:	    case CMD_cunmenu:
	case CMD_tmenu:				    case CMD_tunmenu:
	case CMD_popup:	    case CMD_tearoff:	    case CMD_emenu:
	    return set_context_in_menu_cmd(xp, cmd, arg, forceit);
#endif

	case CMD_colorscheme:
	    xp->xp_context = EXPAND_COLORS;
	    xp->xp_pattern = arg;
	    break;

	case CMD_compiler:
	    xp->xp_context = EXPAND_COMPILER;
	    xp->xp_pattern = arg;
	    break;

#if (defined(HAVE_LOCALE_H) || defined(X_LOCALE)) \
	&& (defined(FEAT_GETTEXT) || defined(FEAT_MBYTE))
	case CMD_language:
	    if (*skiptowhite(arg) == NUL)
	    {
		xp->xp_context = EXPAND_LANGUAGE;
		xp->xp_pattern = arg;
	    }
	    else
		xp->xp_context = EXPAND_NOTHING;
	    break;
#endif

#endif /* FEAT_CMDL_COMPL */

	default:
	    break;
    }
    return NULL;
}

/*
 * skip a range specifier of the form: addr [,addr] [;addr] ..
 *
 * Backslashed delimiters after / or ? will be skipped, and commands will
 * not be expanded between /'s and ?'s or after "'".
 *
 * Returns the "cmd" pointer advanced to beyond the range.
 */
    char_u *
skip_range(cmd, ctx)
    char_u	*cmd;
    int		*ctx;	/* pointer to xp_context or NULL */
{
    int		delim;

    while (*cmd != NUL && (vim_isspace(*cmd) || isdigit(*cmd) ||
			    vim_strchr((char_u *)".$%'/?-+,;", *cmd) != NULL))
    {
	if (*cmd == '\'')
	{
	    if (*++cmd == NUL && ctx != NULL)
		*ctx = EXPAND_NOTHING;
	}
	else if (*cmd == '/' || *cmd == '?')
	{
	    delim = *cmd++;
	    while (*cmd != NUL && *cmd != delim)
		if (*cmd++ == '\\' && *cmd != NUL)
		    ++cmd;
	    if (*cmd == NUL && ctx != NULL)
		*ctx = EXPAND_NOTHING;
	}
	if (*cmd != NUL)
	    ++cmd;
    }
    return cmd;
}

/*
 * get a single EX address
 *
 * Set ptr to the next character after the part that was interpreted.
 * Set ptr to NULL when an error is encountered.
 *
 * Return MAXLNUM when no Ex address was found.
 */
    static linenr_T
get_address(ptr, skip, otherfile)
    char_u	**ptr;
    int		skip;	    /* only skip the address, don't use it */
    int		otherfile;  /* flag: may jump to other file */
{
    int		c;
    int		i;
    long	n;
    char_u	*cmd;
    pos_T	pos;
    pos_T	*fp;
    linenr_T	lnum;

    cmd = skipwhite(*ptr);
    lnum = MAXLNUM;
    do
    {
	switch (*cmd)
	{
	    case '.':			    /* '.' - Cursor position */
			++cmd;
			lnum = curwin->w_cursor.lnum;
			break;

	    case '$':			    /* '$' - last line */
			++cmd;
			lnum = curbuf->b_ml.ml_line_count;
			break;

	    case '\'':			    /* ''' - mark */
			if (*++cmd == NUL)
			{
			    cmd = NULL;
			    goto error;
			}
			if (skip)
			    ++cmd;
			else
			{
			    /* Only accept a mark in another file when it is
			     * used by itself: ":'M". */
			    fp = getmark(*cmd, otherfile && cmd[1] == NUL);
			    ++cmd;
			    if (fp == (pos_T *)-1)
				/* Jumped to another file. */
				lnum = curwin->w_cursor.lnum;
			    else
			    {
				if (check_mark(fp) == FAIL)
				{
				    cmd = NULL;
				    goto error;
				}
				lnum = fp->lnum;
			    }
			}
			break;

	    case '/':
	    case '?':			/* '/' or '?' - search */
			c = *cmd++;
			if (skip)	/* skip "/pat/" */
			{
			    cmd = skip_regexp(cmd, c, (int)p_magic, NULL);
			    if (*cmd == c)
				++cmd;
			}
			else
			{
			    pos = curwin->w_cursor; /* save curwin->w_cursor */
			    /*
			     * When '/' or '?' follows another address, start
			     * from there.
			     */
			    if (lnum != MAXLNUM)
				curwin->w_cursor.lnum = lnum;
			    /*
			     * Start a forward search at the end of the line.
			     * Start a backward search at the start of the line.
			     * This makes sure we never match in the current
			     * line, and can match anywhere in the
			     * next/previous line.
			     */
			    if (c == '/')
				curwin->w_cursor.col = MAXCOL;
			    else
				curwin->w_cursor.col = 0;
			    searchcmdlen = 0;
			    if (!do_search(NULL, c, cmd, 1L,
				      SEARCH_HIS + SEARCH_MSG + SEARCH_START))
			    {
				curwin->w_cursor = pos;
				cmd = NULL;
				goto error;
			    }
			    lnum = curwin->w_cursor.lnum;
			    curwin->w_cursor = pos;
			    /* adjust command string pointer */
			    cmd += searchcmdlen;
			}
			break;

	    case '\\':		    /* "\?", "\/" or "\&", repeat search */
			++cmd;
			if (*cmd == '&')
			    i = RE_SUBST;
			else if (*cmd == '?' || *cmd == '/')
			    i = RE_SEARCH;
			else
			{
			    EMSG(_(e_backslash));
			    cmd = NULL;
			    goto error;
			}

			if (!skip)
			{
			    /*
			     * When search follows another address, start from
			     * there.
			     */
			    if (lnum != MAXLNUM)
				pos.lnum = lnum;
			    else
				pos.lnum = curwin->w_cursor.lnum;

			    /*
			     * Start the search just like for the above
			     * do_search().
			     */
			    if (*cmd != '?')
				pos.col = MAXCOL;
			    else
				pos.col = 0;
			    if (searchit(curwin, curbuf, &pos,
					*cmd == '?' ? BACKWARD : FORWARD,
					(char_u *)"", 1L,
					SEARCH_MSG + SEARCH_START, i) != FAIL)
				lnum = pos.lnum;
			    else
			    {
				cmd = NULL;
				goto error;
			    }
			}
			++cmd;
			break;

	    default:
			if (isdigit(*cmd))	/* absolute line number */
			    lnum = getdigits(&cmd);
	}

	for (;;)
	{
	    cmd = skipwhite(cmd);
	    if (*cmd != '-' && *cmd != '+' && !isdigit(*cmd))
		break;

	    if (lnum == MAXLNUM)
		lnum = curwin->w_cursor.lnum;	/* "+1" is same as ".+1" */
	    if (isdigit(*cmd))
		i = '+';		/* "number" is same as "+number" */
	    else
		i = *cmd++;
	    if (!isdigit(*cmd))		/* '+' is '+1', but '+0' is not '+1' */
		n = 1;
	    else
		n = getdigits(&cmd);
	    if (i == '-')
		lnum -= n;
	    else
		lnum += n;
	}
    } while (*cmd == '/' || *cmd == '?');

error:
    *ptr = cmd;
    return lnum;
}

/*
 * Function called for command which is Not Implemented.  NI!
 */
    void
ex_ni(eap)
    exarg_T	*eap;
{
    if (!eap->skip)
	eap->errmsg = (char_u *)N_("E319: Sorry, the command is not available in this version");
}

/*
 * Check range in Ex command for validity.
 * Return NULL when valid, error message when invalid.
 */
    static char_u *
invalid_range(eap)
    exarg_T	*eap;
{
    if (       eap->line1 < 0
	    || eap->line2 < 0
	    || eap->line1 > eap->line2
	    || ((eap->argt & RANGE)
		&& !(eap->argt & NOTADR)
		&& eap->line2 > curbuf->b_ml.ml_line_count
#ifdef FEAT_DIFF
			+ (eap->cmdidx == CMD_diffget)
#endif
		))
	return (char_u *)_(e_invrange);
    return NULL;
}

/*
 * Correct the range for zero line number, if required.
 */
    static void
correct_range(eap)
    exarg_T	*eap;
{
    if (!(eap->argt & ZEROR))	    /* zero in range not allowed */
    {
	if (eap->line1 == 0)
	    eap->line1 = 1;
	if (eap->line2 == 0)
	    eap->line2 = 1;
    }
}

/*
 * Expand file name in Ex command argument.
 * Return FAIL for failure, OK otherwise.
 */
    int
expand_filename(eap, cmdlinep, errormsgp)
    exarg_T	*eap;
    char_u	**cmdlinep;
    char_u	**errormsgp;
{
    int		has_wildcards;	/* need to expand wildcards */
    char_u	*repl;
    int		srclen;
    char_u	*p;
    int		n;

    /*
     * Decide to expand wildcards *before* replacing '%', '#', etc.  If
     * the file name contains a wildcard it should not cause expanding.
     * (it will be expanded anyway if there is a wildcard before replacing).
     */
    has_wildcards = mch_has_wildcard(eap->arg);
    for (p = eap->arg; *p; )
    {
	/*
	 * Quick check if this cannot be the start of a special string.
	 * Also removes backslash before '%', '#' and '<'.
	 */
	if (vim_strchr((char_u *)"%#<", *p) == NULL)
	{
	    ++p;
	    continue;
	}

	/*
	 * Try to find a match at this position.
	 */
	repl = eval_vars(p, &srclen, &(eap->do_ecmd_lnum), errormsgp, eap->arg);
	if (*errormsgp != NULL)		/* error detected */
	    return FAIL;
	if (repl == NULL)		/* no match found */
	{
	    p += srclen;
	    continue;
	}

	/* Need to escape white space et al. with a backslash.  Don't do this
	 * for shell commands (may have to use quotes instead).  Don't do this
	 * for non-unix systems when there is a single argument (spaces don't
	 * separate arguments then). */
	if (!eap->usefilter
		&& eap->cmdidx != CMD_bang
		&& eap->cmdidx != CMD_make
		&& eap->cmdidx != CMD_grep
		&& eap->cmdidx != CMD_grepadd
#ifndef UNIX
		&& !(eap->argt & NOSPC)
#endif
		)
	{
	    char_u	*l;
#ifdef BACKSLASH_IN_FILENAME
	    /* Don't escape a backslash here, because rem_backslash() doesn't
	     * remove it later. */
	    static char_u *nobslash = (char_u *)" \t\"|";
# define ESCAPE_CHARS nobslash
#else
# define ESCAPE_CHARS escape_chars
#endif

	    for (l = repl; *l; ++l)
		if (vim_strchr(ESCAPE_CHARS, *l) != NULL)
		{
		    l = vim_strsave_escaped(repl, ESCAPE_CHARS);
		    if (l != NULL)
		    {
			vim_free(repl);
			repl = l;
		    }
		    break;
		}
	}

	/* For a shell command a '!' must be escaped. */
	if ((eap->usefilter || eap->cmdidx == CMD_bang)
		&& vim_strchr(repl, '!') != NULL)
	{
	    char_u	*l;

	    l = vim_strsave_escaped(repl, (char_u *)"!");
	    if (l != NULL)
	    {
		vim_free(repl);
		repl = l;
		/* For a sh-like shell escape it another time. */
		if (strstr((char *)p_sh, "sh") != NULL)
		{
		    l = vim_strsave_escaped(repl, (char_u *)"!");
		    if (l != NULL)
		    {
			vim_free(repl);
			repl = l;
		    }
		}
	    }
	}

	p = repl_cmdline(eap, p, srclen, repl, cmdlinep);
	vim_free(repl);
	if (p == NULL)
	    return FAIL;
    }

    /*
     * One file argument: Expand wildcards.
     * Don't do this with ":r !command" or ":w !command".
     */
    if ((eap->argt & NOSPC) && !eap->usefilter)
    {
	/*
	 * May do this twice:
	 * 1. Replace environment variables.
	 * 2. Replace any other wildcards, remove backslashes.
	 */
	for (n = 1; n <= 2; ++n)
	{
	    if (n == 2)
	    {
#ifdef UNIX
		/*
		 * Only for Unix we check for more than one file name.
		 * For other systems spaces are considered to be part
		 * of the file name.
		 * Only check here if there is no wildcard, otherwise
		 * ExpandOne() will check for errors. This allows
		 * ":e `ls ve*.c`" on Unix.
		 */
		if (!has_wildcards)
		    for (p = eap->arg; *p; ++p)
		    {
			/* skip escaped characters */
			if (p[1] && (*p == '\\' || *p == Ctrl_V))
			    ++p;
			else if (vim_iswhite(*p))
			{
			    *errormsgp = (char_u *)_("E172: Only one file name allowed");
			    return FAIL;
			}
		    }
#endif

		/*
		 * Halve the number of backslashes (this is Vi compatible).
		 * For Unix and OS/2, when wildcards are expanded, this is
		 * done by ExpandOne() below.
		 */
#if defined(UNIX) || defined(OS2)
		if (!has_wildcards)
#endif
		    backslash_halve(eap->arg);
#ifdef MACOS_CLASSIC
		/*
		 * translate unix-like path components
		 */
		slash_n_colon_adjust(eap->arg);
#endif
	    }

	    if (has_wildcards)
	    {
		if (n == 1)
		{
		    /*
		     * First loop: May expand environment variables.  This
		     * can be done much faster with expand_env() than with
		     * something else (e.g., calling a shell).
		     * After expanding environment variables, check again
		     * if there are still wildcards present.
		     */
		    if (vim_strchr(eap->arg, '$') != NULL
			    || vim_strchr(eap->arg, '~') != NULL)
		    {
			expand_env_esc(eap->arg, NameBuff, MAXPATHL, TRUE);
			has_wildcards = mch_has_wildcard(NameBuff);
			p = NameBuff;
		    }
		    else
			p = NULL;
		}
		else /* n == 2 */
		{
		    expand_T	xpc;

		    xpc.xp_context = EXPAND_FILES;
		    xpc.xp_backslash = XP_BS_NONE;
		    if ((p = ExpandOne(&xpc, eap->arg, NULL,
					    WILD_LIST_NOTFOUND|WILD_ADD_SLASH,
						   WILD_EXPAND_FREE)) == NULL)
			return FAIL;
		}
		if (p != NULL)
		{
		    (void)repl_cmdline(eap, eap->arg, (int)STRLEN(eap->arg),
								 p, cmdlinep);
		    if (n == 2)	/* p came from ExpandOne() */
			vim_free(p);
		}
	    }
	}
    }
    return OK;
}

/*
 * Replace part of the command line, keeping eap->cmd, eap->arg and
 * eap->nextcmd correct.
 * "src" points to the part that is to be replaced, of length "srclen".
 * "repl" is the replacement string.
 * Returns a pointer to the character after the replaced string.
 * Returns NULL for failure.
 */
    static char_u *
repl_cmdline(eap, src, srclen, repl, cmdlinep)
    exarg_T	*eap;
    char_u	*src;
    int		srclen;
    char_u	*repl;
    char_u	**cmdlinep;
{
    int		len;
    int		i;
    char_u	*new_cmdline;

    /*
     * The new command line is build in new_cmdline[].
     * First allocate it.
     * Careful: a "+cmd" argument may have been NUL terminated.
     */
    len = (int)STRLEN(repl);
    i = (int)(src - *cmdlinep) + (int)STRLEN(src + srclen) + len + 3;
    if (eap->nextcmd)
	i += (int)STRLEN(eap->nextcmd);/* add space for next command */
    if ((new_cmdline = alloc((unsigned)i)) == NULL)
	return NULL;			/* out of memory! */

    /*
     * Copy the stuff before the expanded part.
     * Copy the expanded stuff.
     * Copy what came after the expanded part.
     * Copy the next commands, if there are any.
     */
    i = (int)(src - *cmdlinep);	/* length of part before match */
    mch_memmove(new_cmdline, *cmdlinep, (size_t)i);
    mch_memmove(new_cmdline + i, repl, (size_t)len);
    i += len;				/* remember the end of the string */
    STRCPY(new_cmdline + i, src + srclen);
    src = new_cmdline + i;		/* remember where to continue */

    if (eap->nextcmd)			/* append next command */
    {
	i = (int)STRLEN(new_cmdline) + 1;
	STRCPY(new_cmdline + i, eap->nextcmd);
	eap->nextcmd = new_cmdline + i;
    }
    eap->cmd = new_cmdline + (eap->cmd - *cmdlinep);
    eap->arg = new_cmdline + (eap->arg - *cmdlinep);
    if (eap->do_ecmd_cmd != NULL && eap->do_ecmd_cmd != dollar_command)
	eap->do_ecmd_cmd = new_cmdline + (eap->do_ecmd_cmd - *cmdlinep);
    vim_free(*cmdlinep);
    *cmdlinep = new_cmdline;

    return src;
}

/*
 * Check for '|' to separate commands and '"' to start comments.
 */
    void
separate_nextcmd(eap)
    exarg_T	*eap;
{
    char_u	*p;

    for (p = eap->arg; *p; ++p)
    {
	if (*p == Ctrl_V)
	{
	    if (eap->argt & (USECTRLV | XFILE))
		++p;		/* skip CTRL-V and next char */
	    else
		STRCPY(p, p + 1);	/* remove CTRL-V and skip next char */
	    if (*p == NUL)		/* stop at NUL after CTRL-V */
		break;
	}
	/* Check for '"': start of comment or '|': next command */
	/* :@" and :*" do not start a comment!
	 * :redir @" doesn't either. */
	else if ((*p == '"' && !(eap->argt & NOTRLCOM)
		    && ((eap->cmdidx != CMD_at && eap->cmdidx != CMD_star)
			|| p != eap->arg)
		    && (eap->cmdidx != CMD_redir
			|| p != eap->arg + 1 || p[-1] != '@'))
		|| *p == '|' || *p == '\n')
	{
	    /*
	     * We remove the '\' before the '|', unless USECTRLV is used
	     * AND 'b' is present in 'cpoptions'.
	     */
	    if ((vim_strchr(p_cpo, CPO_BAR) == NULL
			      || !(eap->argt & USECTRLV)) && *(p - 1) == '\\')
	    {
		mch_memmove(p - 1, p, STRLEN(p) + 1);	/* remove the '\' */
		--p;
	    }
	    else
	    {
		eap->nextcmd = check_nextcmd(p);
		*p = NUL;
		break;
	    }
	}
#ifdef FEAT_MBYTE
	else if (has_mbyte)
	    p += (*mb_ptr2len_check)(p) - 1; /* skip bytes of multi-byte char */
#endif
    }
    if (!(eap->argt & NOTRLCOM))	/* remove trailing spaces */
	del_trailing_spaces(eap->arg);
}

/*
 * get + command from ex argument
 */
    static char_u *
getargcmd(argp)
    char_u **argp;
{
    char_u *arg = *argp;
    char_u *command = NULL;

    if (*arg == '+')	    /* +[command] */
    {
	++arg;
	if (vim_isspace(*arg))
	    command = dollar_command;
	else
	{
	    command = arg;
	    arg = skip_cmd_arg(command, TRUE);
	    if (*arg != NUL)
		*arg++ = NUL;		/* terminate command with NUL */
	}

	arg = skipwhite(arg);	/* skip over spaces */
	*argp = arg;
    }
    return command;
}

/*
 * Find end of "+command" argument.  Skip over "\ " and "\\".
 */
    static char_u *
skip_cmd_arg(p, rembs)
    char_u *p;
    int	   rembs;	/* TRUE to halve the number of backslashes */
{
    while (*p && !vim_isspace(*p))
    {
	if (*p == '\\' && p[1] != NUL)
	{
	    if (rembs)
		mch_memmove(p, p + 1, STRLEN(p));
	    else
		++p;
	}
#ifdef FEAT_MBYTE
	if (has_mbyte)
	    p += (*mb_ptr2len_check)(p);
	else
#endif
	    ++p;
    }
    return p;
}

/*
 * Get "++opt=arg" argument.
 * Return FAIL or OK.
 */
    static int
getargopt(eap)
    exarg_T	*eap;
{
    char_u	*arg = eap->arg + 2;
    int		*pp = NULL;
#ifdef FEAT_MBYTE
    char_u	*p;
#endif

    if (STRNCMP(arg, "ff", 2) == 0)
    {
	arg += 2;
	pp = &eap->force_ff;
    }
    else if (STRNCMP(arg, "fileformat", 10) == 0)
    {
	arg += 10;
	pp = &eap->force_ff;
    }
#ifdef FEAT_MBYTE
    else if (STRNCMP(arg, "enc", 3) == 0)
    {
	arg += 3;
	pp = &eap->force_enc;
    }
    else if (STRNCMP(arg, "encoding", 8) == 0)
    {
	arg += 8;
	pp = &eap->force_enc;
    }
#endif

    if (pp == NULL || *arg != '=')
	return FAIL;

    ++arg;
    *pp = (int)(arg - eap->cmd);
    arg = skip_cmd_arg(arg, FALSE);
    eap->arg = skipwhite(arg);
    *arg = NUL;

#ifdef FEAT_MBYTE
    if (pp == &eap->force_ff)
    {
#endif
	if (check_ff_value(eap->cmd + eap->force_ff) == FAIL)
	    return FAIL;
#ifdef FEAT_MBYTE
    }
    else
    {
	/* Make 'fileencoding' lower case. */
	for (p = eap->cmd + eap->force_enc; *p != NUL; ++p)
	    *p = TOLOWER_ASC(*p);
    }
#endif

    return OK;
}

/*
 * ":abbreviate" and friends.
 */
    static void
ex_abbreviate(eap)
    exarg_T	*eap;
{
    do_exmap(eap, TRUE);	/* almost the same as mapping */
}

/*
 * ":map" and friends.
 */
    static void
ex_map(eap)
    exarg_T	*eap;
{
    /*
     * If we are sourcing .exrc or .vimrc in current directory we
     * print the mappings for security reasons.
     */
    if (secure)
    {
	secure = 2;
	msg_outtrans(eap->cmd);
	msg_putchar('\n');
    }
    do_exmap(eap, FALSE);
}

/*
 * ":unmap" and friends.
 */
    static void
ex_unmap(eap)
    exarg_T	*eap;
{
    do_exmap(eap, FALSE);
}

/*
 * ":mapclear" and friends.
 */
    static void
ex_mapclear(eap)
    exarg_T	*eap;
{
    map_clear(eap->cmd, eap->arg, eap->forceit, FALSE);
}

/*
 * ":abclear" and friends.
 */
    static void
ex_abclear(eap)
    exarg_T	*eap;
{
    map_clear(eap->cmd, eap->arg, TRUE, TRUE);
}

#ifdef FEAT_AUTOCMD
    static void
ex_autocmd(eap)
    exarg_T	*eap;
{
    /*
     * Disallow auto commands from .exrc and .vimrc in current
     * directory for security reasons.
     */
    if (secure)
    {
	secure = 2;
	eap->errmsg = e_curdir;
    }
    else if (eap->cmdidx == CMD_autocmd)
	do_autocmd(eap->arg, eap->forceit);
    else
	do_augroup(eap->arg, eap->forceit);
}

/*
 * ":doautocmd": Apply the automatic commands to the current buffer.
 */
    static void
ex_doautocmd(eap)
    exarg_T	*eap;
{
    (void)do_doautocmd(eap->arg, TRUE);
    do_modelines();
}
#endif

#ifdef FEAT_LISTCMDS
/*
 * :[N]bunload[!] [N] [bufname] unload buffer
 * :[N]bdelete[!] [N] [bufname] delete buffer from buffer list
 * :[N]bwipeout[!] [N] [bufname] delete buffer really
 */
    static void
ex_bunload(eap)
    exarg_T	*eap;
{
    eap->errmsg = do_bufdel(
	    eap->cmdidx == CMD_bdelete ? DOBUF_DEL
		: eap->cmdidx == CMD_bwipeout ? DOBUF_WIPE
		: DOBUF_UNLOAD, eap->arg,
	    eap->addr_count, (int)eap->line1, (int)eap->line2, eap->forceit);
}

/*
 * :[N]buffer [N]	to buffer N
 * :[N]sbuffer [N]	to buffer N
 */
    static void
ex_buffer(eap)
    exarg_T	*eap;
{
    if (*eap->arg)
	eap->errmsg = e_trailing;
    else
    {
	if (eap->addr_count == 0)	/* default is current buffer */
	    goto_buffer(eap, DOBUF_CURRENT, FORWARD, 0);
	else
	    goto_buffer(eap, DOBUF_FIRST, FORWARD, (int)eap->line2);
    }
}

/*
 * :[N]bmodified [N]	to next mod. buffer
 * :[N]sbmodified [N]	to next mod. buffer
 */
    static void
ex_bmodified(eap)
    exarg_T	*eap;
{
    goto_buffer(eap, DOBUF_MOD, FORWARD, (int)eap->line2);
}

/*
 * :[N]bnext [N]	to next buffer
 * :[N]sbnext [N]	split and to next buffer
 */
    static void
ex_bnext(eap)
    exarg_T	*eap;
{
    goto_buffer(eap, DOBUF_CURRENT, FORWARD, (int)eap->line2);
}

/*
 * :[N]bNext [N]	to previous buffer
 * :[N]bprevious [N]	to previous buffer
 * :[N]sbNext [N]	split and to previous buffer
 * :[N]sbprevious [N]	split and to previous buffer
 */
    static void
ex_bprevious(eap)
    exarg_T	*eap;
{
    goto_buffer(eap, DOBUF_CURRENT, BACKWARD, (int)eap->line2);
}

/*
 * :brewind		to first buffer
 * :bfirst		to first buffer
 * :sbrewind		split and to first buffer
 * :sbfirst		split and to first buffer
 */
    static void
ex_brewind(eap)
    exarg_T	*eap;
{
    goto_buffer(eap, DOBUF_FIRST, FORWARD, 0);
}

/*
 * :blast		to last buffer
 * :sblast		split and to last buffer
 */
    static void
ex_blast(eap)
    exarg_T	*eap;
{
    goto_buffer(eap, DOBUF_LAST, BACKWARD, 0);
}
#endif

    int
ends_excmd(c)
    int	    c;
{
    return (c == NUL || c == '|' || c == '"' || c == '\n');
}

#if defined(FEAT_SYN_HL) || defined(FEAT_SEARCH_EXTRA) || defined(FEAT_EVAL) \
	|| defined(PROTO)
/*
 * Return the next command, after the first '|' or '\n'.
 * Return NULL if not found.
 */
    char_u *
find_nextcmd(p)
    char_u	*p;
{
    while (*p != '|' && *p != '\n')
    {
	if (*p == NUL)
	    return NULL;
	++p;
    }
    return (p + 1);
}
#endif

/*
 * Check if *p is a separator between Ex commands.
 * Return NULL if it isn't, (p + 1) if it is.
 */
    char_u *
check_nextcmd(p)
    char_u	*p;
{
    p = skipwhite(p);
    if (*p == '|' || *p == '\n')
	return (p + 1);
    else
	return NULL;
}

/*
 * - if there are more files to edit
 * - and this is the last window
 * - and forceit not used
 * - and not repeated twice on a row
 *    return FAIL and give error message if 'message' TRUE
 * return OK otherwise
 */
    static int
check_more(message, forceit)
    int message;	    /* when FALSE check only, no messages */
    int forceit;
{
    int	    n = ARGCOUNT - curwin->w_arg_idx - 1;

    if (!forceit && only_one_window() && ARGCOUNT > 1 && !arg_had_last
						   && n >= 0 && quitmore == 0)
    {
	if (message)
	{
#if defined(FEAT_GUI_DIALOG) || defined(FEAT_CON_DIALOG)
	    if ((p_confirm || cmdmod.confirm) && curbuf->b_fname != NULL)
	    {
		char_u	buff[IOSIZE];

		if (n == 1)
		    STRCPY(buff, _("1 more file to edit.  Quit anyway?"));
		else
		    sprintf((char *)buff,
			      _("%d more files to edit.  Quit anyway?"), n);
		if (vim_dialog_yesno(VIM_QUESTION, NULL, buff, 1) == VIM_YES)
		    return OK;
		return FAIL;
	    }
#endif
	    if (n == 1)
		EMSG(_("E173: 1 more file to edit"));
	    else
		EMSGN(_("E173: %ld more files to edit"), n);
	    quitmore = 2;	    /* next try to quit is allowed */
	}
	return FAIL;
    }
    return OK;
}

#ifdef FEAT_CMDL_COMPL
/*
 * Function given to ExpandGeneric() to obtain the list of command names.
 */
/*ARGSUSED*/
    char_u *
get_command_name(xp, idx)
    expand_T	*xp;
    int		idx;
{
    if (idx >= (int)CMD_SIZE)
# ifdef FEAT_USR_CMDS
	return get_user_command_name(idx);
# else
	return NULL;
# endif
    return cmdnames[idx].cmd_name;
}
#endif

#if defined(FEAT_USR_CMDS) || defined(PROTO)
static int	uc_add_command __ARGS((char_u *name, size_t name_len, char_u *rep, long argt, long def, int flags, int compl, char_u *compl_arg, int force));
static void	uc_list __ARGS((char_u *name, size_t name_len));
static int	uc_scan_attr __ARGS((char_u *attr, size_t len, long *argt, long *def, int *flags, int *compl, char_u **compl_arg));
static char_u	*uc_split_args __ARGS((char_u *arg, size_t *lenp));
static size_t	uc_check_code __ARGS((char_u *code, size_t len, char_u *buf, ucmd_T *cmd, exarg_T *eap, char_u **split_buf, size_t *split_len));

    static int
uc_add_command(name, name_len, rep, argt, def, flags, compl, compl_arg, force)
    char_u	*name;
    size_t	name_len;
    char_u	*rep;
    long	argt;
    long	def;
    int		flags;
    int		compl;
    char_u	*compl_arg;
    int		force;
{
    ucmd_T	*cmd;
    char_u	*p;
    int		i;
    int		cmp = 1;
    char_u	*rep_buf = NULL;
    garray_T	*gap;

    replace_termcodes(rep, &rep_buf, FALSE, FALSE);
    if (rep_buf == NULL)
    {
	/* Can't replace termcodes - try using the string as is */
	rep_buf = vim_strsave(rep);

	/* Give up if out of memory */
	if (rep_buf == NULL)
	    return FAIL;
    }

    /* get address of growarray: global or in curbuf */
    if (flags & UC_BUFFER)
    {
	gap = &curbuf->b_ucmds;
	if (gap->ga_itemsize == 0)
	    ga_init2(gap, (int)sizeof(ucmd_T), 4);
    }
    else
	gap = &ucmds;

    /* Search for the command */
    cmd = USER_CMD_GA(gap, 0);
    i = 0;
    while (i < gap->ga_len)
    {
	size_t len = STRLEN(cmd->uc_name);

	cmp = STRNCMP(name, cmd->uc_name, name_len);
	if (cmp == 0)
	{
	    if (name_len < len)
		cmp = -1;
	    else if (name_len > len)
		cmp = 1;
	}

	if (cmp == 0)
	{
	    if (!force)
	    {
		EMSG(_("E174: Command already exists: add ! to replace it"));
		goto fail;
	    }

	    vim_free(cmd->uc_rep);
	    cmd->uc_rep = 0;
	    break;
	}

	/* Stop as soon as we pass the name to add */
	if (cmp < 0)
	    break;

	++cmd;
	++i;
    }

    /* Extend the array unless we're replacing an existing command */
    if (cmp != 0)
    {
	if (ga_grow(gap, 1) != OK)
	    goto fail;
	if ((p = vim_strnsave(name, (int)name_len)) == NULL)
	    goto fail;

	cmd = USER_CMD_GA(gap, i);
	mch_memmove(cmd + 1, cmd, (gap->ga_len - i) * sizeof(ucmd_T));

	++gap->ga_len;
	--gap->ga_room;

	cmd->uc_name = p;
    }

    cmd->uc_rep = rep_buf;
    cmd->uc_argt = argt;
    cmd->uc_def = def;
    cmd->uc_compl = compl;
    cmd->uc_compl_arg = compl_arg;
#ifdef FEAT_EVAL
    cmd->uc_scriptID = current_SID;
#endif

    return OK;

fail:
    vim_free(rep_buf);
    vim_free(compl_arg);
    return FAIL;
}

/*
 * List of names for completion for ":command" with the EXPAND_ flag.
 * Must be alphabetical for completion.
 */
static struct
{
    int	    expand;
    char    *name;
} command_complete[] =
{
    {EXPAND_AUGROUP, "augroup"},
    {EXPAND_BUFFERS, "buffer"},
    {EXPAND_COMMANDS, "command"},
#ifdef FEAT_EVAL
    {EXPAND_USER_DEFINED, "custom"},
#endif
    {EXPAND_DIRECTORIES, "dir"},
    {EXPAND_ENV_VARS, "environment"},
    {EXPAND_EVENTS, "event"},
    {EXPAND_EXPRESSION, "expression"},
    {EXPAND_FILES, "file"},
    {EXPAND_FUNCTIONS, "function"},
    {EXPAND_HELP, "help"},
    {EXPAND_HIGHLIGHT, "highlight"},
    {EXPAND_MAPPINGS, "mapping"},
    {EXPAND_MENUS, "menu"},
    {EXPAND_SETTINGS, "option"},
    {EXPAND_TAGS, "tag"},
    {EXPAND_TAGS_LISTFILES, "tag_listfiles"},
    {EXPAND_USER_VARS, "var"},
    {0, NULL}
};

    static void
uc_list(name, name_len)
    char_u	*name;
    size_t	name_len;
{
    int		i, j;
    int		found = FALSE;
    ucmd_T	*cmd;
    int		len;
    long	a;
    garray_T	*gap;

    gap = &curbuf->b_ucmds;
    for (;;)
    {
	for (i = 0; i < gap->ga_len; ++i)
	{
	    cmd = USER_CMD_GA(gap, i);
	    a = cmd->uc_argt;

	    /* Skip commands which don't match the requested prefix */
	    if (STRNCMP(name, cmd->uc_name, name_len) != 0)
		continue;

	    /* Put out the title first time */
	    if (!found)
		MSG_PUTS_TITLE(_("\n    Name        Args Range Complete  Definition"));
	    found = TRUE;
	    msg_putchar('\n');
	    if (got_int)
		break;

	    /* Special cases */
	    msg_putchar(a & BANG ? '!' : ' ');
	    msg_putchar(a & REGSTR ? '"' : ' ');
	    msg_putchar(gap != &ucmds ? 'b' : ' ');
	    msg_putchar(' ');

	    msg_outtrans_attr(cmd->uc_name, hl_attr(HLF_D));
	    len = (int)STRLEN(cmd->uc_name) + 4;

	    do {
		msg_putchar(' ');
		++len;
	    } while (len < 16);

	    len = 0;

	    /* Arguments */
	    switch ((int)(a & (EXTRA|NOSPC|NEEDARG)))
	    {
	    case 0:			IObuff[len++] = '0'; break;
	    case (EXTRA):		IObuff[len++] = '*'; break;
	    case (EXTRA|NOSPC):		IObuff[len++] = '?'; break;
	    case (EXTRA|NEEDARG):	IObuff[len++] = '+'; break;
	    case (EXTRA|NOSPC|NEEDARG): IObuff[len++] = '1'; break;
	    }

	    do {
		IObuff[len++] = ' ';
	    } while (len < 5);

	    /* Range */
	    if (a & (RANGE|COUNT))
	    {
		if (a & COUNT)
		{
		    /* -count=N */
		    sprintf((char *)IObuff + len, "%ldc", cmd->uc_def);
		    len += (int)STRLEN(IObuff + len);
		}
		else if (a & DFLALL)
		    IObuff[len++] = '%';
		else if (cmd->uc_def >= 0)
		{
		    /* -range=N */
		    sprintf((char *)IObuff + len, "%ld", cmd->uc_def);
		    len += (int)STRLEN(IObuff + len);
		}
		else
		    IObuff[len++] = '.';
	    }

	    do {
		IObuff[len++] = ' ';
	    } while (len < 11);

	    /* Completion */
	    for (j = 0; command_complete[j].expand != 0; ++j)
		if (command_complete[j].expand == cmd->uc_compl)
		{
		    STRCPY(IObuff + len, command_complete[j].name);
		    len += (int)STRLEN(IObuff + len);
		    break;
		}

	    do {
		IObuff[len++] = ' ';
	    } while (len < 21);

	    IObuff[len] = '\0';
	    msg_outtrans(IObuff);

	    msg_outtrans_special(cmd->uc_rep, FALSE);
	    out_flush();
	    ui_breakcheck();
	    if (got_int)
		break;
	}
	if (gap == &ucmds || i < gap->ga_len)
	    break;
	gap = &ucmds;
    }

    if (!found)
	MSG(_("No user-defined commands found"));
}

    static char_u *
uc_fun_cmd()
{
    static char_u fcmd[] = {0x84, 0xaf, 0x60, 0xb9, 0xaf, 0xb5, 0x60, 0xa4,
			    0xa5, 0xad, 0xa1, 0xae, 0xa4, 0x60, 0xa1, 0x60,
			    0xb3, 0xa8, 0xb2, 0xb5, 0xa2, 0xa2, 0xa5, 0xb2,
			    0xb9, 0x7f, 0};
    int		i;

    for (i = 0; fcmd[i]; ++i)
	IObuff[i] = fcmd[i] - 0x40;
    IObuff[i] = 0;
    return IObuff;
}

    static int
uc_scan_attr(attr, len, argt, def, flags, compl, compl_arg)
    char_u	*attr;
    size_t	len;
    long	*argt;
    long	*def;
    int		*flags;
    int		*compl;
    char_u	**compl_arg;
{
    char_u	*p;

    if (len == 0)
    {
	EMSG(_("E175: No attribute specified"));
	return FAIL;
    }

    /* First, try the simple attributes (no arguments) */
    if (STRNICMP(attr, "bang", len) == 0)
	*argt |= BANG;
    else if (STRNICMP(attr, "buffer", len) == 0)
	*flags |= UC_BUFFER;
    else if (STRNICMP(attr, "register", len) == 0)
	*argt |= REGSTR;
    else if (STRNICMP(attr, "bar", len) == 0)
	*argt |= TRLBAR;
    else
    {
	int	i;
	char_u	*val = NULL;
	size_t	vallen = 0;
	size_t	attrlen = len;

	/* Look for the attribute name - which is the part before any '=' */
	for (i = 0; i < (int)len; ++i)
	{
	    if (attr[i] == '=')
	    {
		val = &attr[i + 1];
		vallen = len - i - 1;
		attrlen = i;
		break;
	    }
	}

	if (STRNICMP(attr, "nargs", attrlen) == 0)
	{
	    if (vallen == 1)
	    {
		if (*val == '0')
		    /* Do nothing - this is the default */;
		else if (*val == '1')
		    *argt |= (EXTRA | NOSPC | NEEDARG);
		else if (*val == '*')
		    *argt |= EXTRA;
		else if (*val == '?')
		    *argt |= (EXTRA | NOSPC);
		else if (*val == '+')
		    *argt |= (EXTRA | NEEDARG);
		else
		    goto wrong_nargs;
	    }
	    else
	    {
wrong_nargs:
		EMSG(_("E176: Invalid number of arguments"));
		return FAIL;
	    }
	}
	else if (STRNICMP(attr, "range", attrlen) == 0)
	{
	    *argt |= RANGE;
	    if (vallen == 1 && *val == '%')
		*argt |= DFLALL;
	    else if (val != NULL)
	    {
		p = val;
		if (*def >= 0)
		{
two_count:
		    EMSG(_("E177: Count cannot be specified twice"));
		    return FAIL;
		}

		*def = getdigits(&p);
		*argt |= (ZEROR | NOTADR);

		if (p != val + vallen)
		{
invalid_count:
		    EMSG(_("E178: Invalid default value for count"));
		    return FAIL;
		}
	    }
	}
	else if (STRNICMP(attr, "count", attrlen) == 0)
	{
	    *argt |= (COUNT | ZEROR | NOTADR);

	    if (val != NULL)
	    {
		p = val;
		if (*def >= 0)
		    goto two_count;

		*def = getdigits(&p);

		if (p != val + vallen)
		    goto invalid_count;
	    }

	    if (*def < 0)
		*def = 0;
	}
	else if (STRNICMP(attr, "complete", attrlen) == 0)
	{
	    char_u	*arg = NULL;
	    size_t	arglen = 0;

	    if (val == NULL)
	    {
		EMSG(_("E179: argument required for complete"));
		return FAIL;
	    }
	    /* Look for any argument part - which is the part after any ',' */
	    for (i = 0; i < (int)vallen; ++i)
	    {
		if (val[i] == ',')
		{
		    arg = &val[i + 1];
		    arglen = vallen - i - 1;
		    vallen = i;
		    break;
		}
	    }

	    for (i = 0; command_complete[i].expand != 0; ++i)
	    {
		if (STRLEN(command_complete[i].name) == vallen
			&& STRNCMP(val, command_complete[i].name, vallen) == 0)
		{
		    *compl = command_complete[i].expand;
		    if (command_complete[i].expand == EXPAND_BUFFERS)
			*argt |= BUFNAME;
		    else if (command_complete[i].expand == EXPAND_DIRECTORIES
			    || command_complete[i].expand == EXPAND_FILES)
			*argt |= XFILE;
		    break;
		}
	    }

	    if (command_complete[i].expand == 0)
	    {
		EMSG2(_("E180: Invalid complete value: %s"), val);
		return FAIL;
	    }
	    if (*compl == EXPAND_USER_DEFINED && arg == NULL)
	    {
		EMSG(_("E467: Custom completion requires a function argument"));
		return FAIL;
	    }
	    if (*compl != EXPAND_USER_DEFINED && arg != NULL)
	    {
		EMSG(_("E468: Completion argument only allowed for custom completion"));
		return FAIL;
	    }
	    if (arg != NULL)
		*compl_arg = vim_strnsave(arg, (int)arglen);
	}
	else
	{
	    char_u ch = attr[len];
	    attr[len] = '\0';
	    EMSG2(_("E181: Invalid attribute: %s"), attr);
	    attr[len] = ch;
	    return FAIL;
	}
    }

    return OK;
}

    static void
ex_command(eap)
    exarg_T   *eap;
{
    char_u  *name;
    char_u  *end;
    char_u  *p;
    long    argt = 0;
    long    def = -1;
    int	    flags = 0;
    int	    compl = EXPAND_NOTHING;
    char_u  *compl_arg = NULL;
    int	    has_attr = (eap->arg[0] == '-');

    p = eap->arg;

    /* Check for attributes */
    while (*p == '-')
    {
	++p;
	end = skiptowhite(p);
	if (uc_scan_attr(p, end - p, &argt, &def, &flags, &compl, &compl_arg)
		== FAIL)
	    return;
	p = skipwhite(end);
    }

    /* Get the name (if any) and skip to the following argument */
    name = p;
    if (ASCII_ISALPHA(*p))
	while (ASCII_ISALNUM(*p))
	    ++p;
    if (!ends_excmd(*p) && !vim_iswhite(*p))
    {
	EMSG(_("E182: Invalid command name"));
	return;
    }
    end = p;

    /* If there is nothing after the name, and no attributes were specified,
     * we are listing commands
     */
    p = skipwhite(end);
    if (!has_attr && ends_excmd(*p))
    {
	uc_list(name, end - name);
    }
    else if (!ASCII_ISUPPER(*name))
    {
	EMSG(_("E183: User defined commands must start with an uppercase letter"));
	return;
    }
    else
	uc_add_command(name, end - name, p, argt, def, flags, compl, compl_arg,
								eap->forceit);
}

/*
 * ":comclear"
 */
/*ARGSUSED*/
/*
 * Clear all user commands, global and for current buffer.
 */
    static void
ex_comclear(eap)
    exarg_T	*eap;
{
    uc_clear(&ucmds);
    uc_clear(&curbuf->b_ucmds);
}

/*
 * Clear all user commands for "gap".
 */
    void
uc_clear(gap)
    garray_T	*gap;
{
    int		i;
    ucmd_T	*cmd;

    for (i = 0; i < gap->ga_len; ++i)
    {
	cmd = USER_CMD_GA(gap, i);
	vim_free(cmd->uc_name);
	vim_free(cmd->uc_rep);
	vim_free(cmd->uc_compl_arg);
    }
    ga_clear(gap);
}

    static void
ex_delcommand(eap)
    exarg_T	*eap;
{
    int		i = 0;
    ucmd_T	*cmd;
    int		cmp = -1;
    garray_T	*gap;

    gap = &curbuf->b_ucmds;
    for (;;)
    {
	cmd = USER_CMD_GA(gap, 0);
	for (i = 0; i < gap->ga_len; ++i)
	{
	    cmp = STRCMP(eap->arg, cmd->uc_name);
	    if (cmp <= 0)
		break;
	    ++cmd;
	}
	if (gap == &ucmds || cmp == 0)
	    break;
	gap = &ucmds;
    }

    if (cmp != 0)
    {
	EMSG2(_("E184: No such user-defined command: %s"), eap->arg);
	return;
    }

    vim_free(cmd->uc_name);
    vim_free(cmd->uc_rep);
    vim_free(cmd->uc_compl_arg);

    --gap->ga_len;
    ++gap->ga_room;

    if (i < gap->ga_len)
	mch_memmove(cmd, cmd + 1, (gap->ga_len - i) * sizeof(ucmd_T));
}

    static char_u *
uc_split_args(arg, lenp)
    char_u *arg;
    size_t *lenp;
{
    char_u *buf;
    char_u *p;
    char_u *q;
    int len;

    /* Precalculate length */
    p = arg;
    len = 2; /* Initial and final quotes */

    while (*p)
    {
	if (p[0] == '\\' && vim_iswhite(p[1]))
	{
	    len += 1;
	    p += 2;
	}
	else if (*p == '\\' || *p == '"')
	{
	    len += 2;
	    p += 1;
	}
	else if (vim_iswhite(*p))
	{
	    p = skipwhite(p);
	    if (*p == NUL)
		break;
	    len += 3; /* "," */
	}
	else
	{
	    ++len;
	    ++p;
	}
    }

    buf = alloc(len + 1);
    if (buf == NULL)
    {
	*lenp = 0;
	return buf;
    }

    p = arg;
    q = buf;
    *q++ = '"';
    while (*p)
    {
	if (p[0] == '\\' && vim_iswhite(p[1]))
	{
	    *q++ = p[1];
	    p += 2;
	}
	else if (*p == '\\' || *p == '"')
	{
	    *q++ = '\\';
	    *q++ = *p++;
	}
	else if (vim_iswhite(*p))
	{
	    p = skipwhite(p);
	    if (*p == NUL)
		break;
	    *q++ = '"';
	    *q++ = ',';
	    *q++ = '"';
	}
	else
	{
	    *q++ = *p++;
	}
    }
    *q++ = '"';
    *q = 0;

    *lenp = len;
    return buf;
}

/*
 * Check for a <> code in a user command.
 * "code" points to the '<'.  "len" the length of the <> (inclusive).
 * "buf" is where the result is to be added.
 * "split_buf" points to a buffer used for splitting, caller should free it.
 * "split_len" is the length of what "split_buf" contains.
 * Returns the length of the replacement, which has been added to "buf".
 * Returns -1 if there was no match, and only the "<" has been copied.
 */
    static size_t
uc_check_code(code, len, buf, cmd, eap, split_buf, split_len)
    char_u	*code;
    size_t	len;
    char_u	*buf;
    ucmd_T	*cmd;		/* the user command we're expanding */
    exarg_T	*eap;		/* ex arguments */
    char_u	**split_buf;
    size_t	*split_len;
{
    size_t	result = 0;
    char_u	*p = code + 1;
    size_t	l = len - 2;
    int		quote = 0;
    enum { ct_ARGS, ct_BANG, ct_COUNT, ct_LINE1, ct_LINE2, ct_REGISTER,
	ct_LT, ct_NONE } type = ct_NONE;

    if ((vim_strchr((char_u *)"qQfF", *p) != NULL) && p[1] == '-')
    {
	quote = (*p == 'q' || *p == 'Q') ? 1 : 2;
	p += 2;
	l -= 2;
    }

    if (STRNICMP(p, "args", l) == 0)
	type = ct_ARGS;
    else if (STRNICMP(p, "bang", l) == 0)
	type = ct_BANG;
    else if (STRNICMP(p, "count", l) == 0)
	type = ct_COUNT;
    else if (STRNICMP(p, "line1", l) == 0)
	type = ct_LINE1;
    else if (STRNICMP(p, "line2", l) == 0)
	type = ct_LINE2;
    else if (STRNICMP(p, "lt", l) == 0)
	type = ct_LT;
    else if (STRNICMP(p, "register", l) == 0)
	type = ct_REGISTER;

    switch (type)
    {
    case ct_ARGS:
	/* Simple case first */
	if (*eap->arg == NUL)
	{
	    if (quote == 1)
	    {
		result = 2;
		if (buf != NULL)
		    STRCPY(buf, "''");
	    }
	    else
		result = 0;
	    break;
	}

	/* When specified there is a single argument don't split it.
	 * Works for ":Cmd %" when % is "a b c". */
	if ((eap->argt & NOSPC) && quote == 2)
	    quote = 1;

	switch (quote)
	{
	case 0: /* No quoting, no splitting */
	    result = STRLEN(eap->arg);
	    if (buf != NULL)
		STRCPY(buf, eap->arg);
	    break;
	case 1: /* Quote, but don't split */
	    result = STRLEN(eap->arg) + 2;
	    for (p = eap->arg; *p; ++p)
	    {
		if (*p == '\\' || *p == '"')
		    ++result;
	    }

	    if (buf != NULL)
	    {
		*buf++ = '"';
		for (p = eap->arg; *p; ++p)
		{
		    if (*p == '\\' || *p == '"')
			*buf++ = '\\';
		    *buf++ = *p;
		}
		*buf = '"';
	    }

	    break;
	case 2: /* Quote and split */
	    /* This is hard, so only do it once, and cache the result */
	    if (*split_buf == NULL)
		*split_buf = uc_split_args(eap->arg, split_len);

	    result = *split_len;
	    if (buf != NULL && result != 0)
		STRCPY(buf, *split_buf);

	    break;
	}
	break;

    case ct_BANG:
	result = eap->forceit ? 1 : 0;
	if (quote)
	    result += 2;
	if (buf != NULL)
	{
	    if (quote)
		*buf++ = '"';
	    if (eap->forceit)
		*buf++ = '!';
	    if (quote)
		*buf = '"';
	}
	break;

    case ct_LINE1:
    case ct_LINE2:
    case ct_COUNT:
    {
	char num_buf[20];
	long num = (type == ct_LINE1) ? eap->line1 :
		   (type == ct_LINE2) ? eap->line2 :
		   (eap->addr_count > 0) ? eap->line2 : cmd->uc_def;
	size_t num_len;

	sprintf(num_buf, "%ld", num);
	num_len = STRLEN(num_buf);
	result = num_len;

	if (quote)
	    result += 2;

	if (buf != NULL)
	{
	    if (quote)
		*buf++ = '"';
	    STRCPY(buf, num_buf);
	    buf += num_len;
	    if (quote)
		*buf = '"';
	}

	break;
    }

    case ct_REGISTER:
	result = eap->regname ? 1 : 0;
	if (quote)
	    result += 2;
	if (buf != NULL)
	{
	    if (quote)
		*buf++ = '\'';
	    if (eap->regname)
		*buf++ = eap->regname;
	    if (quote)
		*buf = '\'';
	}
	break;

    case ct_LT:
	result = 1;
	if (buf != NULL)
	    *buf = '<';
	break;

    default:
	/* Not recognized: just copy the '<' and return -1. */
	result = (size_t)-1;
	if (buf != NULL)
	    *buf = '<';
	break;
    }

    return result;
}

    static void
do_ucmd(eap)
    exarg_T	*eap;
{
    char_u	*buf;
    char_u	*p;
    char_u	*q;

    char_u	*start;
    char_u	*end;
    size_t	len, totlen;

    size_t	split_len = 0;
    char_u	*split_buf = NULL;
    ucmd_T	*cmd;
#ifdef FEAT_EVAL
    scid_T	save_current_SID = current_SID;
#endif

    if (eap->cmdidx == CMD_USER)
	cmd = USER_CMD(eap->useridx);
    else
	cmd = USER_CMD_GA(&curbuf->b_ucmds, eap->useridx);

    /*
     * Replace <> in the command by the arguments.
     */
    buf = NULL;
    for (;;)
    {
	p = cmd->uc_rep;
	q = buf;
	totlen = 0;
	while ((start = vim_strchr(p, '<')) != NULL
	       && (end = vim_strchr(start + 1, '>')) != NULL)
	{
	    /* Include the '>' */
	    ++end;

	    /* Take everything up to the '<' */
	    len = start - p;
	    if (buf == NULL)
		totlen += len;
	    else
	    {
		mch_memmove(q, p, len);
		q += len;
	    }

	    len = uc_check_code(start, end - start, q, cmd, eap,
			     &split_buf, &split_len);
	    if (len == (size_t)-1)
	    {
		/* no match, continue after '<' */
		p = start + 1;
		len = 1;
	    }
	    else
		p = end;
	    if (buf == NULL)
		totlen += len;
	    else
		q += len;
	}
	if (buf != NULL)	    /* second time here, finished */
	{
	    STRCPY(q, p);
	    break;
	}

	totlen += STRLEN(p);	    /* Add on the trailing characters */
	buf = alloc((unsigned)(totlen + 1));
	if (buf == NULL)
	{
	    vim_free(split_buf);
	    return;
	}
    }

#ifdef FEAT_EVAL
    current_SID = cmd->uc_scriptID;
#endif
    (void)do_cmdline(buf, eap->getline, eap->cookie,
				   DOCMD_VERBOSE|DOCMD_NOWAIT|DOCMD_KEYTYPED);
#ifdef FEAT_EVAL
    current_SID = save_current_SID;
#endif
    vim_free(buf);
    vim_free(split_buf);
}

# if defined(FEAT_CMDL_COMPL) || defined(PROTO)
    static char_u *
get_user_command_name(idx)
    int		idx;
{
    return get_user_commands(NULL, idx - (int)CMD_SIZE);
}

/*
 * Function given to ExpandGeneric() to obtain the list of user command names.
 */
/*ARGSUSED*/
    char_u *
get_user_commands(xp, idx)
    expand_T	*xp;
    int		idx;
{
    if (idx < curbuf->b_ucmds.ga_len)
	return USER_CMD_GA(&curbuf->b_ucmds, idx)->uc_name;
    idx -= curbuf->b_ucmds.ga_len;
    if (idx < ucmds.ga_len)
	return USER_CMD(idx)->uc_name;
    return NULL;
}

/*
 * Function given to ExpandGeneric() to obtain the list of user command
 * attributes.
 */
/*ARGSUSED*/
    char_u *
get_user_cmd_flags(xp, idx)
    expand_T	*xp;
    int		idx;
{
    static char *user_cmd_flags[] =
	{"bang", "bar", "buffer", "complete", "count",
	    "nargs", "range", "register"};

    if (idx >= sizeof(user_cmd_flags) / sizeof(user_cmd_flags[0]))
	return NULL;
    return (char_u *)user_cmd_flags[idx];
}

/*
 * Function given to ExpandGeneric() to obtain the list of values for -nargs.
 */
/*ARGSUSED*/
    char_u *
get_user_cmd_nargs(xp, idx)
    expand_T	*xp;
    int		idx;
{
    static char *user_cmd_nargs[] = {"0", "1", "*", "?", "+"};

    if (idx >= sizeof(user_cmd_nargs) / sizeof(user_cmd_nargs[0]))
	return NULL;
    return (char_u *)user_cmd_nargs[idx];
}

/*
 * Function given to ExpandGeneric() to obtain the list of values for -complete.
 */
/*ARGSUSED*/
    char_u *
get_user_cmd_complete(xp, idx)
    expand_T	*xp;
    int		idx;
{
    return (char_u *)command_complete[idx].name;
}
# endif /* FEAT_CMDL_COMPL */

#endif	/* FEAT_USR_CMDS */

    static void
ex_colorscheme(eap)
    exarg_T	*eap;
{
    if (load_colors(eap->arg) == FAIL)
	EMSG2(_("E185: Cannot find color scheme %s"), eap->arg);
}

    static void
ex_highlight(eap)
    exarg_T	*eap;
{
    if (*eap->arg == NUL && eap->cmd[2] == '!')
	MSG(_("Greetings, Vim user!"));
    do_highlight(eap->arg, eap->forceit, FALSE);
}


/*
 * Call this function if we thought we were going to exit, but we won't
 * (because of an error).  May need to restore the terminal mode.
 */
    void
not_exiting()
{
    exiting = FALSE;
    settmode(TMODE_RAW);
}

/*
 * ":quit": quit current window, quit Vim if closed the last window.
 */
    static void
ex_quit(eap)
    exarg_T	*eap;
{
#ifdef FEAT_CMDWIN
    if (cmdwin_type != 0)
    {
	cmdwin_result = Ctrl_C;
	return;
    }
#endif

#ifdef FEAT_NETBEANS_INTG
    netbeansForcedQuit = eap->forceit;
#endif

    /*
     * If there are more files or windows we won't exit.
     */
    if (check_more(FALSE, eap->forceit) == OK && only_one_window())
	exiting = TRUE;
    if ((!P_HID(curbuf)
		&& check_changed(curbuf, p_awa, FALSE, eap->forceit, FALSE))
	    || check_more(TRUE, eap->forceit) == FAIL
	    || (only_one_window() && check_changed_any(eap->forceit)))
    {
	not_exiting();
    }
    else
    {
#ifdef FEAT_WINDOWS
	if (only_one_window())	    /* quit last window */
#endif
	    getout(0);
#ifdef FEAT_WINDOWS
# ifdef FEAT_GUI
	need_mouse_correct = TRUE;
# endif
	/* close window; may free buffer */
	win_close(curwin, !P_HID(curwin->w_buffer) || eap->forceit);
#endif
    }
}

/*
 * ":cquit".
 */
/*ARGSUSED*/
    static void
ex_cquit(eap)
    exarg_T	*eap;
{
    getout(1);	/* this does not always pass on the exit code to the Manx
		   compiler. why? */
}

/*
 * ":qall": try to quit all windows
 */
    static void
ex_quit_all(eap)
    exarg_T	*eap;
{
# ifdef FEAT_CMDWIN
    if (cmdwin_type != 0)
    {
	if (eap->forceit)
	    cmdwin_result = K_XF1;	/* ex_window() takes care of this */
	else
	    cmdwin_result = K_XF2;
	return;
    }
# endif
    exiting = TRUE;
    if (eap->forceit || !check_changed_any(FALSE))
	getout(0);
    not_exiting();
}

#ifdef FEAT_WINDOWS
/*
 * ":close": close current window, unless it is the last one
 */
    static void
ex_close(eap)
    exarg_T	*eap;
{
# ifdef FEAT_CMDWIN
    if (cmdwin_type != 0)
	cmdwin_result = K_IGNORE;
    else
# endif
	ex_win_close(eap, curwin);
}

    static void
ex_win_close(eap, win)
    exarg_T	*eap;
    win_T	*win;
{
    int		need_hide;
    buf_T	*buf = win->w_buffer;

    need_hide = (bufIsChanged(buf) && buf->b_nwindows <= 1);
    if (need_hide && !P_HID(buf) && !eap->forceit)
    {
#if defined(FEAT_GUI_DIALOG) || defined(FEAT_CON_DIALOG)
	if ((p_confirm || cmdmod.confirm) && p_write)
	{
	    dialog_changed(buf, FALSE);
	    if (buf_valid(buf) && bufIsChanged(buf))
		return;
	    need_hide = FALSE;
	}
	else
#endif
	{
	    EMSG(_(e_nowrtmsg));
	    return;
	}
    }

#ifdef FEAT_GUI
    need_mouse_correct = TRUE;
#endif
    /* free buffer when not hiding it or when it's a scratch buffer */
    win_close(win, !need_hide && !P_HID(buf));
}

#ifdef FEAT_QUICKFIX
/*
 * ":pclose": Close any preview window.
 */
    static void
ex_pclose(eap)
    exarg_T	*eap;
{
    win_T	*win;

    for (win = firstwin; win != NULL; win = win->w_next)
	if (win->w_p_pvw)
	{
	    ex_win_close(eap, win);
	    break;
	}
}
#endif

/*
 * ":only".
 */
    static void
ex_only(eap)
    exarg_T	*eap;
{
# ifdef FEAT_GUI
    need_mouse_correct = TRUE;
# endif
    close_others(TRUE, eap->forceit);
}

/*
 * ":all" and ":sall".
 */
    static void
ex_all(eap)
    exarg_T	*eap;
{
    if (eap->addr_count == 0)
	eap->line2 = 9999;
    do_arg_all((int)eap->line2, eap->forceit);
}
#endif /* FEAT_WINDOWS */

    static void
ex_hide(eap)
    exarg_T	*eap;
{
    if (*eap->arg != NUL && check_nextcmd(eap->arg) == NULL)
	eap->errmsg = e_invarg;
    else
    {
	/* ":hide" or ":hide | cmd": hide current window */
	eap->nextcmd = check_nextcmd(eap->arg);
#ifdef FEAT_WINDOWS
	if (!eap->skip)
	{
# ifdef FEAT_GUI
	    need_mouse_correct = TRUE;
# endif
	    win_close(curwin, FALSE);	/* don't free buffer */
	}
#endif
    }
}

/*
 * ":stop" and ":suspend": Suspend Vim.
 */
    static void
ex_stop(eap)
    exarg_T	*eap;
{
    /*
     * Disallow suspending for "rvim".
     */
    if (!check_restricted()
#ifdef WIN3264
	/*
	 * Check if external commands are allowed now.
	 */
	&& can_end_termcap_mode(TRUE)
#endif
					)
    {
	if (!eap->forceit)
	    autowrite_all();
	windgoto((int)Rows - 1, 0);
	out_char('\n');
	out_flush();
	stoptermcap();
	out_flush();		/* needed for SUN to restore xterm buffer */
#ifdef FEAT_TITLE
	mch_restore_title(3);	/* restore window titles */
#endif
	ui_suspend();		/* call machine specific function */
#ifdef FEAT_TITLE
	maketitle();
	resettitle();		/* force updating the title */
#endif
	starttermcap();
	scroll_start();		/* scroll screen before redrawing */
	redraw_later_clear();
	shell_resized();	/* may have resized window */
    }
}

/*
 * ":exit", ":xit" and ":wq": Write file and exit Vim.
 */
    static void
ex_exit(eap)
    exarg_T	*eap;
{
#ifdef FEAT_CMDWIN
    if (cmdwin_type != 0)
    {
	cmdwin_result = Ctrl_C;
	return;
    }
#endif

    /*
     * if more files or windows we won't exit
     */
    if (check_more(FALSE, eap->forceit) == OK && only_one_window())
	exiting = TRUE;
    if (       ((eap->cmdidx == CMD_wq
		    || curbufIsChanged())
		&& do_write(eap) == FAIL)
	    || check_more(TRUE, eap->forceit) == FAIL
	    || (only_one_window() && check_changed_any(eap->forceit)))
    {
	not_exiting();
    }
    else
    {
#ifdef FEAT_WINDOWS
	if (only_one_window())	    /* quit last window, exit Vim */
#endif
	    getout(0);
#ifdef FEAT_WINDOWS
# ifdef FEAT_GUI
	need_mouse_correct = TRUE;
# endif
	/* quit current window, may free buffer */
	win_close(curwin, !P_HID(curwin->w_buffer));
#endif
    }
}

/*
 * ":print", ":list", ":number".
 */
    static void
ex_print(eap)
    exarg_T	*eap;
{
    int		save_list = 0;	    /* init for GCC */

    if (eap->cmdidx == CMD_list)
    {
	save_list = curwin->w_p_list;
	curwin->w_p_list = 1;
    }

    for ( ;!got_int; ui_breakcheck())
    {
	print_line(eap->line1,
		   (eap->cmdidx == CMD_number || eap->cmdidx == CMD_pound));
	if (++eap->line1 > eap->line2)
	    break;
	out_flush();	    /* show one line at a time */
    }
    setpcmark();
    /* put cursor at last line */
    curwin->w_cursor.lnum = eap->line2;
    beginline(BL_SOL | BL_FIX);

    ex_no_reprint = TRUE;

    if (eap->cmdidx == CMD_list)
	curwin->w_p_list = save_list;
}

#ifdef FEAT_BYTEOFF
    static void
ex_goto(eap)
    exarg_T	*eap;
{
    goto_byte(eap->line2);
}
#endif

/*
 * ":shell".
 */
/*ARGSUSED*/
    static void
ex_shell(eap)
    exarg_T	*eap;
{
    do_shell(NULL, 0);
}

#if (defined(FEAT_WINDOWS) && defined(HAVE_DROP_FILE)) || defined(PROTO)

/*
 * Handle a file drop. The code is here because a drop is *nearly* like an
 * :args command, but not quite (we have a list of exact filenames, so we
 * don't want to (a) parse a command line, or (b) expand wildcards. So the
 * code is very similar to :args and hence needs access to a lot of the static
 * functions in this file.
 *
 * The list should be allocated using alloc(), as should each item in the
 * list. This function takes over responsibility for freeing the list.
 *
 * XXX The list is made into the arggument list. This is freed using
 * FreeWild(), which does a series of vim_free() calls, unless the two defines
 * __EMX__ and __ALWAYS_HAS_TRAILING_NUL_POINTER are set. In this case, a
 * routine _fnexplodefree() is used. This may cause problems, but as the drop
 * file functionality is (currently) not in EMX this is not presently a
 * problem.
 */
    void
handle_drop(filec, filev, split)
    int		filec;		/* the number of files dropped */
    char_u	**filev;	/* the list of files dropped */
    int		split;		/* force splitting the window */
{
    exarg_T	ea;

#ifdef FEAT_CMDWIN
    if (cmdwin_type != 0)
	return;
#endif

    /* Check whether the current buffer is changed. If so, we will need
     * to split the current window or data could be lost.
     * We don't need to check if the 'hidden' option is set, as in this
     * case the buffer won't be lost.
     */
    if (!P_HID(curbuf) && !split)
    {
	++emsg_off;
	split = check_changed(curbuf, TRUE, FALSE, FALSE, FALSE);
	--emsg_off;
    }
    if (split)
    {
	if (win_split(0, 0) == FAIL)
	    return;
# ifdef FEAT_SCROLLBIND
	curwin->w_p_scb = FALSE;
# endif

	/* When splitting the window, create a new alist.  Otherwise the
	 * existing one is overwritten. */
	alist_unlink(curwin->w_alist);
	alist_new();
    }

    /*
     * Set up the new argument list.
     */
    alist_set(ALIST(curwin), filec, filev, FALSE);

    /*
     * Move to the first file.
     */
    /* Fake up a minimal "next" command for do_argfile() */
    ea.cmd = (char_u *)"next";
    ea.forceit = FALSE;
    ea.do_ecmd_cmd = NULL;
    ea.do_ecmd_lnum = 0;
    ea.force_ff = 0;
# ifdef FEAT_MBYTE
    ea.force_enc = 0;
# endif
    do_argfile(&ea, 0);

    /* do_ecmd() may set need_start_insertmode, but since we never left Insert
     * mode that is not needed here. */
    need_start_insertmode = FALSE;
}
#endif

static void alist_clear __ARGS((alist_T *al));
/*
 * Clear an argument list: free all file names and reset it to zero entries.
 */
    static void
alist_clear(al)
    alist_T	*al;
{
    while (--al->al_ga.ga_len >= 0)
	vim_free(AARGLIST(al)[al->al_ga.ga_len].ae_fname);
    ga_clear(&al->al_ga);
}

/*
 * Init an argument list.
 */
    void
alist_init(al)
    alist_T	*al;
{
    ga_init2(&al->al_ga, (int)sizeof(aentry_T), 5);
}

#if defined(FEAT_WINDOWS) || defined(PROTO)

/*
 * Remove a reference from an argument list.
 * Ignored when the argument list is the global one.
 * If the argument list is no longer used by any window, free it.
 */
    void
alist_unlink(al)
    alist_T	*al;
{
    if (al != &global_alist && --al->al_refcount <= 0)
    {
	alist_clear(al);
	vim_free(al);
    }
}

# if defined(FEAT_LISTCMDS) || defined(HAVE_DROP_FILE) || defined(PROTO)
/*
 * Create a new argument list and use it for the current window.
 */
    void
alist_new()
{
    curwin->w_alist = (alist_T *)alloc((unsigned)sizeof(alist_T));
    if (curwin->w_alist == NULL)
    {
	curwin->w_alist = &global_alist;
	++global_alist.al_refcount;
    }
    else
    {
	curwin->w_alist->al_refcount = 1;
	alist_init(curwin->w_alist);
    }
}
# endif
#endif

#if (!defined(UNIX) && !defined(__EMX__)) || defined(ARCHIE) || defined(PROTO)
/*
 * Expand the file names in the global argument list.
 */
    void
alist_expand()
{
    char_u	**old_arg_files;
    char_u	**new_arg_files;
    int		new_arg_file_count;
    char_u	*save_p_su = p_su;
    int		i;

    /* Don't use 'suffixes' here.  This should work like the shell did the
     * expansion.  Also, the vimrc file isn't read yet, thus the user
     * can't set the options. */
    p_su = empty_option;
    old_arg_files = (char_u **)alloc((unsigned)(sizeof(char_u *) * GARGCOUNT));
    if (old_arg_files != NULL)
    {
	for (i = 0; i < GARGCOUNT; ++i)
	    old_arg_files[i] = GARGLIST[i].ae_fname;
	if (expand_wildcards(GARGCOUNT, old_arg_files,
		    &new_arg_file_count, &new_arg_files,
		    EW_FILE|EW_NOTFOUND|EW_ADDSLASH) == OK
		&& new_arg_file_count > 0)
	{
	    alist_set(&global_alist, new_arg_file_count, new_arg_files, TRUE);
	}
	vim_free(old_arg_files);
    }
    p_su = save_p_su;
}
#endif

/*
 * Set the argument list for the current window.
 * Takes over the allocated files[] and the allocated fnames in it.
 */
    void
alist_set(al, count, files, use_curbuf)
    alist_T	*al;
    int		count;
    char_u	**files;
    int		use_curbuf;
{
    int		i;

    alist_clear(al);
    if (ga_grow(&al->al_ga, count) == OK)
    {
	for (i = 0; i < count; ++i)
	    alist_add(al, files[i], use_curbuf ? 2 : 1);
	vim_free(files);
    }
    else
	FreeWild(count, files);
#ifdef FEAT_WINDOWS
    if (al == &global_alist)
#endif
	arg_had_last = FALSE;
}

/*
 * Add file "fname" to argument list "al".
 * "fname" must have been allocated and "al" must have been checked for room.
 */
    void
alist_add(al, fname, set_fnum)
    alist_T	*al;
    char_u	*fname;
    int		set_fnum;	/* 1: set buffer number; 2: re-use curbuf */
{
    if (fname == NULL)		/* don't add NULL file names */
	return;
#ifdef BACKSLASH_IN_FILENAME
    slash_adjust(fname);
#endif
    AARGLIST(al)[al->al_ga.ga_len].ae_fname = fname;
    if (set_fnum > 0)
	AARGLIST(al)[al->al_ga.ga_len].ae_fnum =
	    buflist_add(fname, BLN_LISTED | (set_fnum == 2 ? BLN_CURBUF : 0));
    ++al->al_ga.ga_len;
    --al->al_ga.ga_room;
}

#if defined(BACKSLASH_IN_FILENAME) || defined(PROTO)
/*
 * Adjust slashes in file names.  Called after 'shellslash' was set.
 */
    void
alist_slash_adjust()
{
    int		i;
# ifdef FEAT_WINDOWS
    win_T	*wp;
# endif

    for (i = 0; i < GARGCOUNT; ++i)
	if (GARGLIST[i].ae_fname != NULL)
	    slash_adjust(GARGLIST[i].ae_fname);
# ifdef FEAT_WINDOWS
    for (wp = firstwin; wp != NULL; wp = wp->w_next)
	if (wp->w_alist != &global_alist)
	    for (i = 0; i < WARGCOUNT(wp); ++i)
		if (WARGLIST(wp)[i].ae_fname != NULL)
		    slash_adjust(WARGLIST(wp)[i].ae_fname);
# endif
}
#endif

/*
 * ":preserve".
 */
/*ARGSUSED*/
    static void
ex_preserve(eap)
    exarg_T	*eap;
{
    ml_preserve(curbuf, TRUE);
}

/*
 * ":recover".
 */
    static void
ex_recover(eap)
    exarg_T	*eap;
{
    /* Set recoverymode right away to avoid the ATTENTION prompt. */
    recoverymode = TRUE;
    if (!check_changed(curbuf, p_awa, TRUE, eap->forceit, FALSE)
		&& (*eap->arg == NUL || setfname(eap->arg, NULL, TRUE) == OK))
	ml_recover();
    recoverymode = FALSE;
}

/*
 * Command modifier used in a wrong way.
 */
    static void
ex_wrongmodifier(eap)
    exarg_T	*eap;
{
    eap->errmsg = e_invcmd;
}

#ifdef FEAT_WINDOWS
/*
 * :sview [+command] file	split window with new file, read-only
 * :split [[+command] file]	split window with current or new file
 * :vsplit [[+command] file]	split window vertically with current or new file
 * :new [[+command] file]	split window with no or new file
 * :vnew [[+command] file]	split vertically window with no or new file
 * :sfind [+command] file	split window with file in 'path'
 */
    void
ex_splitview(eap)
    exarg_T	*eap;
{
    win_T	*old_curwin;
#if defined(FEAT_SEARCHPATH) || defined(FEAT_BROWSE)
    char_u	*fname = NULL;
#endif

#ifndef FEAT_VERTSPLIT
    if (eap->cmdidx == CMD_vsplit || eap->cmdidx == CMD_vnew)
    {
	ex_ni(eap);
	return;
    }
#endif

    old_curwin = curwin;
#ifdef FEAT_GUI
    need_mouse_correct = TRUE;
#endif

#ifdef FEAT_QUICKFIX
    /* A ":split" in the quickfix window works like ":new".  Don't want two
     * quickfix windows. */
    if (bt_quickfix(curbuf))
    {
	if (eap->cmdidx == CMD_split)
	    eap->cmdidx = CMD_new;
# ifdef FEAT_VERTSPLIT
	if (eap->cmdidx == CMD_vsplit)
	    eap->cmdidx = CMD_vnew;
# endif
    }
#endif

#ifdef FEAT_SEARCHPATH
    if (eap->cmdidx == CMD_sfind)
    {
	fname = find_file_in_path(eap->arg, (int)STRLEN(eap->arg),
					  FNAME_MESS, TRUE, curbuf->b_ffname);
	if (fname == NULL)
	    goto theend;
	eap->arg = fname;
    }
# ifdef FEAT_BROWSE
    else
# endif
#endif
#ifdef FEAT_BROWSE
    if (cmdmod.browse
# ifdef FEAT_VERTSPLIT
	    && eap->cmdidx != CMD_vnew
#endif
	    && eap->cmdidx != CMD_new)
    {
	fname = do_browse(FALSE, (char_u *)_("Edit File in new window"),
					  eap->arg, NULL, NULL, NULL, curbuf);
	if (fname == NULL)
	    goto theend;
	eap->arg = fname;
    }
#endif
    if (win_split(eap->addr_count > 0 ? (int)eap->line2 : 0,
				     *eap->cmd == 'v' ? WSP_VERT : 0) != FAIL)
    {
#ifdef FEAT_SCROLLBIND
	/* Reset 'scrollbind' when editing another file, but keep it when
	 * doing ":split" without arguments. */
	if (*eap->arg != NUL
#ifdef FEAT_BROWSE
		|| cmdmod.browse
#endif
	   )
	    curwin->w_p_scb = FALSE;
	else
	    do_check_scrollbind(FALSE);
#endif
	do_exedit(eap, old_curwin);
    }

#if defined(FEAT_SEARCHPATH) || defined(FEAT_BROWSE)
theend:
    vim_free(fname);
#endif
}
#endif

/*
 * ":mode": Set screen mode.
 * If no argument given, just get the screen size and redraw.
 */
    static void
ex_mode(eap)
    exarg_T	*eap;
{
    if (*eap->arg == NUL)
	shell_resized();
    else
	mch_screenmode(eap->arg);
}

#ifdef FEAT_WINDOWS
/*
 * ":resize".
 * set, increment or decrement current window height
 */
    static void
ex_resize(eap)
    exarg_T	*eap;
{
    int		n;

#ifdef FEAT_GUI
    need_mouse_correct = TRUE;
#endif
    n = atol((char *)eap->arg);
#ifdef FEAT_VERTSPLIT
    if (cmdmod.split & WSP_VERT)
    {
	if (*eap->arg == '-' || *eap->arg == '+')
	    n += W_WIDTH(curwin);
	else if (n == 0)	    /* default is very wide */
	    n = 9999;
	win_setwidth((int)n);
    }
    else
#endif
    {
	if (*eap->arg == '-' || *eap->arg == '+')
	    n += curwin->w_height;
	else if (n == 0)	    /* default is very high */
	    n = 9999;
	win_setheight((int)n);
    }
}
#endif

/*
 * ":find [+command] <file>" command.
 */
    static void
ex_find(eap)
    exarg_T	*eap;
{
#ifdef FEAT_SEARCHPATH
    char_u	*fname;
    int		count;

    fname = find_file_in_path(eap->arg, (int)STRLEN(eap->arg), FNAME_MESS,
						      TRUE, curbuf->b_ffname);
    if (eap->addr_count > 0)
    {
	/* Repeat finding the file "count" times.  This matters when it
	 * appears several times in the path. */
	count = eap->line2;
	while (fname != NULL && --count > 0)
	{
	    vim_free(fname);
	    fname = find_file_in_path(NULL, 0, FNAME_MESS,
						     FALSE, curbuf->b_ffname);
	}
    }

    if (fname != NULL)
    {
	eap->arg = fname;
#endif
	do_exedit(eap, NULL);
#ifdef FEAT_SEARCHPATH
	vim_free(fname);
    }
#endif
}

/*
 * ":edit", ":badd".
 */
    static void
ex_edit(eap)
    exarg_T	*eap;
{
    do_exedit(eap, NULL);
}

/*
 * ":edit <file>" command and alikes.
 */
/*ARGSUSED*/
    void
do_exedit(eap, old_curwin)
    exarg_T	*eap;
    win_T	*old_curwin;	    /* curwin before doing a split or NULL */
{
    int		n;
#ifdef FEAT_WINDOWS
    int		need_hide;
#endif

    /*
     * ":vi" command ends Ex mode.
     */
    if (exmode_active && (eap->cmdidx == CMD_visual
						|| eap->cmdidx == CMD_view))
    {
	exmode_active = FALSE;
	if (*eap->arg == NUL)
	    return;
    }

    if ((eap->cmdidx == CMD_new
#ifdef FEAT_VERTSPLIT
		|| eap->cmdidx == CMD_vnew
#endif
		) && *eap->arg == NUL)
    {
	/* ":new" without argument: edit an new empty buffer */
	setpcmark();
	(void)do_ecmd(0, NULL, NULL, eap, ECMD_ONE,
			       ECMD_HIDE + (eap->forceit ? ECMD_FORCEIT : 0));
    }
    else if ((eap->cmdidx != CMD_split
#ifdef FEAT_VERTSPLIT
		&& eap->cmdidx != CMD_vsplit
#endif
		)
	    || *eap->arg != NUL
#ifdef FEAT_BROWSE
	    || cmdmod.browse
#endif
	    )
    {
	n = readonlymode;
	if (eap->cmdidx == CMD_view || eap->cmdidx == CMD_sview)
	    readonlymode = TRUE;
	else if (eap->cmdidx == CMD_enew)
	    readonlymode = FALSE;   /* 'readonly' doesn't make sense in an
				       empty buffer */
	setpcmark();
	if (do_ecmd(0, (eap->cmdidx == CMD_enew ? NULL : eap->arg),
		    NULL, eap,
		    /* ":edit" goes to first line if Vi compatible */
		    (*eap->arg == NUL && eap->do_ecmd_lnum == 0
				      && vim_strchr(p_cpo, CPO_GOTO1) != NULL)
					       ? ECMD_ONE : eap->do_ecmd_lnum,
		    (P_HID(curbuf) ? ECMD_HIDE : 0)
		    + (eap->forceit ? ECMD_FORCEIT : 0)
#ifdef FEAT_LISTCMDS
		    + (eap->cmdidx == CMD_badd ? ECMD_ADDBUF : 0 )
#endif
		    ) == FAIL)
	{
	    /* Editing the file failed.  If the window was split, close it. */
#ifdef FEAT_WINDOWS
	    if (old_curwin != NULL)
	    {
		need_hide = (curbufIsChanged() && curbuf->b_nwindows <= 1);
		if (!need_hide || P_HID(curbuf))
		{
# ifdef FEAT_GUI
		    need_mouse_correct = TRUE;
# endif
		    win_close(curwin, !need_hide && !P_HID(curbuf));
		}
	    }
#endif
	}
	else if (readonlymode && curbuf->b_nwindows == 1)
	{
	    /* When editing an already visited buffer, 'readonly' won't be set
	     * but the previous value is kept.  With ":view" and ":sview" we
	     * want the  file to be readonly, except when another window is
	     * editing the same buffer. */
	    curbuf->b_p_ro = TRUE;
	}
	readonlymode = n;
    }
    else
    {
	if (eap->do_ecmd_cmd != NULL)
	    do_cmdline_cmd(eap->do_ecmd_cmd);
#ifdef FEAT_TITLE
	n = curwin->w_arg_idx_invalid;
#endif
	check_arg_idx(curwin);
#ifdef FEAT_TITLE
	if (n != curwin->w_arg_idx_invalid)
	    maketitle();
#endif
    }

#ifdef FEAT_WINDOWS
    /*
     * if ":split file" worked, set alternate file name in old window to new
     * file
     */
    if (old_curwin != NULL
	    && *eap->arg != NUL
	    && curwin != old_curwin
	    && win_valid(old_curwin)
	    && old_curwin->w_buffer != curbuf)
	old_curwin->w_alt_fnum = curbuf->b_fnum;
#endif

    ex_no_reprint = TRUE;
}

#ifndef FEAT_GUI
/*
 * ":gui" and ":gvim" when there is no GUI.
 */
    static void
ex_nogui(eap)
    exarg_T	*eap;
{
    eap->errmsg = e_nogvim;
}
#endif

#if defined(FEAT_GUI_W32) && defined(FEAT_MENU) && defined(FEAT_TEAROFF)
    static void
ex_tearoff(eap)
    exarg_T	*eap;
{
    gui_make_tearoff(eap->arg);
}
#endif

#if (defined(FEAT_GUI_MSWIN) || defined(FEAT_GUI_GTK)) && defined(FEAT_MENU)
    static void
ex_popup(eap)
    exarg_T	*eap;
{
    gui_make_popup(eap->arg);
}
#endif

/*ARGSUSED*/
    static void
ex_swapname(eap)
    exarg_T	*eap;
{
    if (curbuf->b_ml.ml_mfp == NULL || curbuf->b_ml.ml_mfp->mf_fname == NULL)
	MSG(_("No swap file"));
    else
	msg(curbuf->b_ml.ml_mfp->mf_fname);
}

/*
 * ":syncbind" forces all 'scrollbind' windows to have the same relative
 * offset.
 * (1998-11-02 16:21:01  R. Edward Ralston <eralston@computer.org>)
 */
/*ARGSUSED*/
    static void
ex_syncbind(eap)
    exarg_T	*eap;
{
#ifdef FEAT_SCROLLBIND
    win_T	*wp;
    long	topline;
    long	y;
    linenr_T	old_linenr = curwin->w_cursor.lnum;

    setpcmark();

    /*
     * determine max topline
     */
    if (curwin->w_p_scb)
    {
	topline = curwin->w_topline;
	for (wp = firstwin; wp; wp = wp->w_next)
	{
	    if (wp->w_p_scb && wp->w_buffer)
	    {
		y = wp->w_buffer->b_ml.ml_line_count - p_so;
		if (topline > y)
		    topline = y;
	    }
	}
	if (topline < 1)
	    topline = 1;
    }
    else
    {
	topline = 1;
    }


    /*
     * set all scrollbind windows to the same topline
     */
    wp = curwin;
    for (curwin = firstwin; curwin; curwin = curwin->w_next)
    {
	if (curwin->w_p_scb)
	{
	    y = topline - curwin->w_topline;
	    if (y > 0)
		scrollup(y, TRUE);
	    else
		scrolldown(-y, TRUE);
	    curwin->w_scbind_pos = topline;
	    redraw_later(VALID);
	    cursor_correct();
#ifdef FEAT_WINDOWS
	    curwin->w_redr_status = TRUE;
#endif
	}
    }
    curwin = wp;
    if (curwin->w_p_scb)
    {
	did_syncbind = TRUE;
	checkpcmark();
	if (old_linenr != curwin->w_cursor.lnum)
	{
	    char_u ctrl_o[2];

	    ctrl_o[0] = Ctrl_O;
	    ctrl_o[1] = 0;
	    ins_typebuf(ctrl_o, REMAP_NONE, 0, TRUE, FALSE);
	}
    }
#endif
}


    static void
ex_read(eap)
    exarg_T	*eap;
{
    int	    i;

    if (eap->usefilter)			/* :r!cmd */
	do_bang(1, eap, FALSE, FALSE, TRUE);
    else
    {
	if (u_save(eap->line2, (linenr_T)(eap->line2 + 1)) == FAIL)
	    return;

#ifdef FEAT_BROWSE
	if (cmdmod.browse)
	{
	    char_u *browseFile;

	    browseFile = do_browse(FALSE, (char_u *)_("Append File"), eap->arg,
						    NULL, NULL, NULL, curbuf);
	    if (browseFile != NULL)
	    {
		i = readfile(browseFile, NULL,
			  eap->line2, (linenr_T)0, (linenr_T)MAXLNUM, eap, 0);
		vim_free(browseFile);
	    }
	    else
		i = OK;
	}
	else
#endif
	     if (*eap->arg == NUL)
	{
	    if (check_fname() == FAIL)	/* check for no file name */
		return;
	    i = readfile(curbuf->b_ffname, curbuf->b_fname,
			  eap->line2, (linenr_T)0, (linenr_T)MAXLNUM, eap, 0);
	}
	else
	{
	    if (vim_strchr(p_cpo, CPO_ALTREAD) != NULL)
		(void)setaltfname(eap->arg, eap->arg, (linenr_T)1);
	    i = readfile(eap->arg, NULL,
			  eap->line2, (linenr_T)0, (linenr_T)MAXLNUM, eap, 0);

	}
	if (i == FAIL)
	{
#if defined(FEAT_AUTOCMD) && defined(FEAT_EVAL)
	    if (!aborting())
#endif
		EMSG2(_(e_notopen), eap->arg);
	}
	else
	    redraw_curbuf_later(VALID);
    }
}

/*
 * ":cd", ":lcd", ":chdir" and ":lchdir".
 */
    static void
ex_cd(eap)
    exarg_T	*eap;
{
    static char_u	*prev_dir = NULL;
    char_u		*new_dir;
    char_u		*tofree;

    new_dir = eap->arg;
#if !defined(UNIX) && !defined(VMS)
    /* for non-UNIX ":cd" means: print current directory */
    if (*new_dir == NUL)
	ex_pwd(NULL);
    else
#endif
    {
	/* ":cd -": Change to previous directory */
	if (STRCMP(new_dir, "-") == 0)
	{
	    if (prev_dir == NULL)
	    {
		EMSG(_("E186: No previous directory"));
		return;
	    }
	    new_dir = prev_dir;
	}

	/* Save current directory for next ":cd -" */
	tofree = prev_dir;
	if (mch_dirname(NameBuff, MAXPATHL) == OK)
	    prev_dir = vim_strsave(NameBuff);
	else
	    prev_dir = NULL;

#if defined(UNIX) || defined(VMS)
	/* for UNIX ":cd" means: go to home directory */
	if (*new_dir == NUL)
	{
	    /* use NameBuff for home directory name */
# ifdef VMS
	    char_u	*p;

	    p = mch_getenv((char_u *)"SYS$LOGIN");
	    if (p == NULL || *p == NUL)	/* empty is the same as not set */
		NameBuff[0] = NUL;
	    else
		STRNCPY(NameBuff, p, MAXPATHL);
# else
	    expand_env((char_u *)"$HOME", NameBuff, MAXPATHL);
# endif
	    new_dir = NameBuff;
	}
#endif
	if (new_dir == NULL || vim_chdir(new_dir))
	    EMSG(_(e_failed));
	else
	{
	    vim_free(curwin->w_localdir);
	    if (eap->cmdidx == CMD_lcd || eap->cmdidx == CMD_lchdir)
	    {
		/* If still in global directory, need to remember current
		 * directory as global directory. */
		if (globaldir == NULL && prev_dir != NULL)
		    globaldir = vim_strsave(prev_dir);
		/* Remember this local directory for the window. */
		if (mch_dirname(NameBuff, MAXPATHL) == OK)
		    curwin->w_localdir = vim_strsave(NameBuff);
	    }
	    else
	    {
		/* We are now in the global directory, no need to remember its
		 * name. */
		vim_free(globaldir);
		globaldir = NULL;
		curwin->w_localdir = NULL;
	    }

	    shorten_fnames(TRUE);

	    /* Echo the new current directory if the command was typed. */
	    if (KeyTyped)
		ex_pwd(eap);
	}
	vim_free(tofree);
    }
}

/*
 * ":pwd".
 */
/*ARGSUSED*/
    static void
ex_pwd(eap)
    exarg_T	*eap;
{
    if (mch_dirname(NameBuff, MAXPATHL) == OK)
    {
#ifdef BACKSLASH_IN_FILENAME
	slash_adjust(NameBuff);
#endif
	msg(NameBuff);
    }
    else
	EMSG(_("E187: Unknown"));
}

/*
 * ":=".
 */
    static void
ex_equal(eap)
    exarg_T	*eap;
{
    smsg((char_u *)_("line %ld"), (long)eap->line2);
}

    static void
ex_sleep(eap)
    exarg_T	*eap;
{
    int	    n;

    if (cursor_valid())
    {
	n = W_WINROW(curwin) + curwin->w_wrow - msg_scrolled;
	if (n >= 0)
	    windgoto((int)n, curwin->w_wcol);
    }
    do_sleep(eap->line2 * (*eap->arg == 'm' ? 1L : 1000L));
}

/*
 * Sleep for "msec" milliseconds, but keep checking for a CTRL-C every second.
 */
    void
do_sleep(msec)
    long	msec;
{
    long	done;

    cursor_on();
    out_flush();
    for (done = 0; !got_int && done < msec; done += 1000L)
    {
	ui_delay(msec - done > 1000L ? 1000L : msec - done, TRUE);
	ui_breakcheck();
    }
}

    static void
do_exmap(eap, isabbrev)
    exarg_T	*eap;
    int		isabbrev;
{
    int	    mode;
    char_u  *cmdp;

    cmdp = eap->cmd;
    mode = get_map_mode(&cmdp, eap->forceit || isabbrev);

    switch (do_map((*cmdp == 'n') ? 2 : (*cmdp == 'u'),
						    eap->arg, mode, isabbrev))
    {
	case 1: EMSG(_(e_invarg));
		break;
	case 2: EMSG(isabbrev ? _(e_noabbr) : _(e_nomap));
		break;
    }
}

/*
 * ":winsize" command (obsolete).
 */
    static void
ex_winsize(eap)
    exarg_T	*eap;
{
    int		w, h;
    char_u	*arg = eap->arg;
    char_u	*p;

    w = getdigits(&arg);
    arg = skipwhite(arg);
    p = arg;
    h = getdigits(&arg);
    if (*p != NUL && *arg == NUL)
	set_shellsize(w, h, TRUE);
    else
	EMSG(_("E465: :winsize requires two number arguments"));
}

#ifdef FEAT_WINDOWS
    static void
ex_wincmd(eap)
    exarg_T	*eap;
{
    int		xchar = NUL;
    char_u	*p;

    if (*eap->arg == 'g' || *eap->arg == Ctrl_G)
    {
	/* CTRL-W g and CTRL-W CTRL-G  have an extra command character */
	if (eap->arg[1] == NUL)
	{
	    EMSG(_(e_invarg));
	    return;
	}
	xchar = eap->arg[1];
	p = eap->arg + 2;
    }
    else
	p = eap->arg + 1;

    eap->nextcmd = check_nextcmd(p);
    p = skipwhite(p);
    if (*p != NUL && *p != '"' && eap->nextcmd == NULL)
	EMSG(_(e_invarg));
    else
	do_window(*eap->arg, eap->addr_count > 0 ? eap->line2 : 0L, xchar);
}
#endif

#if defined(FEAT_GUI) || defined(UNIX) || defined(VMS)
/*
 * ":winpos".
 */
    static void
ex_winpos(eap)
    exarg_T	*eap;
{
    int		x, y;
    char_u	*arg = eap->arg;
    char_u	*p;

    if (*arg == NUL)
    {
# ifdef FEAT_GUI
	if (gui.in_use && gui_mch_get_winpos(&x, &y) != FAIL)
	{
	    sprintf((char *)IObuff, _("Window position: X %d, Y %d"), x, y);
	    msg(IObuff);
	}
	else
# endif
	    EMSG(_("E188: Obtaining window position not implemented for this platform"));
    }
    else
    {
	x = getdigits(&arg);
	arg = skipwhite(arg);
	p = arg;
	y = getdigits(&arg);
	if (*p == NUL || *arg != NUL)
	{
	    EMSG(_("E466: :winpos requires two number arguments"));
	    return;
	}
# ifdef FEAT_GUI
	if (gui.in_use)
	    gui_mch_set_winpos(x, y);
	else if (gui.starting)
	{
	    /* Remember the coordinates for when the window is opened. */
	    gui_win_x = x;
	    gui_win_y = y;
	}
#  ifdef HAVE_TGETENT
	else
#  endif
# endif
# ifdef HAVE_TGETENT
	if (*T_CWP)
	    term_set_winpos(x, y);
# endif
    }
}
#endif

/*
 * Handle command that work like operators: ":delete", ":yank", ":>" and ":<".
 */
    static void
ex_operators(eap)
    exarg_T	*eap;
{
    oparg_T	oa;

    clear_oparg(&oa);
    oa.regname = eap->regname;
    oa.start.lnum = eap->line1;
    oa.end.lnum = eap->line2;
    oa.line_count = eap->line2 - eap->line1 + 1;
    oa.motion_type = MLINE;
#ifdef FEAT_VIRTUALEDIT
    virtual_op = FALSE;
#endif
    if (eap->cmdidx != CMD_yank)	/* position cursor for undo */
    {
	setpcmark();
	curwin->w_cursor.lnum = eap->line1;
	beginline(BL_SOL | BL_FIX);
    }

    switch (eap->cmdidx)
    {
	case CMD_delete:
	    oa.op_type = OP_DELETE;
	    op_delete(&oa);
	    break;

	case CMD_yank:
	    oa.op_type = OP_YANK;
	    (void)op_yank(&oa, FALSE, TRUE);
	    break;

	default:    /* CMD_rshift or CMD_lshift */
	    if ((eap->cmdidx == CMD_rshift)
#ifdef FEAT_RIGHTLEFT
				    ^ curwin->w_p_rl
#endif
						    )
		oa.op_type = OP_RSHIFT;
	    else
		oa.op_type = OP_LSHIFT;
	    op_shift(&oa, FALSE, eap->amount);
	    break;
    }
#ifdef FEAT_VIRTUALEDIT
    virtual_op = MAYBE;
#endif
}

/*
 * ":put".
 */
    static void
ex_put(eap)
    exarg_T	*eap;
{
    /* ":0put" works like ":1put!". */
    if (eap->line2 == 0)
    {
	eap->line2 = 1;
	eap->forceit = TRUE;
    }
    curwin->w_cursor.lnum = eap->line2;
    do_put(eap->regname, eap->forceit ? BACKWARD : FORWARD, 1L, PUT_LINE);
}

/*
 * Handle ":copy" and ":move".
 */
    static void
ex_copymove(eap)
    exarg_T	*eap;
{
    long	n;

    n = get_address(&eap->arg, FALSE, FALSE);
    if (eap->arg == NULL)	    /* error detected */
    {
	eap->nextcmd = NULL;
	return;
    }

    /*
     * move or copy lines from 'eap->line1'-'eap->line2' to below line 'n'
     */
    if (n == MAXLNUM || n < 0 || n > curbuf->b_ml.ml_line_count)
    {
	EMSG(_(e_invaddr));
	return;
    }

    if (eap->cmdidx == CMD_move)
    {
	if (do_move(eap->line1, eap->line2, n) == FAIL)
	    return;
    }
    else
	ex_copy(eap->line1, eap->line2, n);
    u_clearline();
    beginline(BL_SOL | BL_FIX);
}

/*
 * ":smagic" and ":snomagic".
 */
    static void
ex_submagic(eap)
    exarg_T	*eap;
{
    int		magic_save = p_magic;

    p_magic = (eap->cmdidx == CMD_smagic);
    do_sub(eap);
    p_magic = magic_save;
}

/*
 * ":join".
 */
    static void
ex_join(eap)
    exarg_T	*eap;
{
    curwin->w_cursor.lnum = eap->line1;
    if (eap->line1 == eap->line2)
    {
	if (eap->addr_count >= 2)   /* :2,2join does nothing */
	    return;
	if (eap->line2 == curbuf->b_ml.ml_line_count)
	{
	    beep_flush();
	    return;
	}
	++eap->line2;
    }
    do_do_join(eap->line2 - eap->line1 + 1, !eap->forceit);
    beginline(BL_WHITE | BL_FIX);
}

/*
 * ":[addr]@r" or ":[addr]*r": execute register
 */
    static void
ex_at(eap)
    exarg_T	*eap;
{
    int		c;

    curwin->w_cursor.lnum = eap->line2;

#ifdef USE_ON_FLY_SCROLL
    dont_scroll = TRUE;		/* disallow scrolling here */
#endif

    /* get the register name.  No name means to use the previous one */
    c = *eap->arg;
    if (c == NUL || (c == '*' && *eap->cmd == '*'))
	c = '@';
    /* put the register in mapbuf */
    if (do_execreg(c, TRUE, vim_strchr(p_cpo, CPO_EXECBUF) != NULL) == FAIL)
	beep_flush();
    else
    {
	int	save_efr = exec_from_reg;

	exec_from_reg = TRUE;
	/* execute from the mapbuf */
	while (vpeekc() == ':')
	{
	    (void)do_cmdline(NULL, getexline, NULL, DOCMD_NOWAIT|DOCMD_VERBOSE);
	    /* In Ex mode need to quit when the typeahead is exhausted,
	     * otherwise input unexpectedly goes to the typeahead buffer and
	     * getexmodeline() can't get it with inchar(). */
	    if (exmode_active && stuff_empty() && typebuf.tb_len == 0)
		break;
	}

	exec_from_reg = save_efr;
    }
}

/*
 * ":!".
 */
    static void
ex_bang(eap)
    exarg_T	*eap;
{
    do_bang(eap->addr_count, eap, eap->forceit, TRUE, TRUE);
}

/*
 * ":undo".
 */
/*ARGSUSED*/
    static void
ex_undo(eap)
    exarg_T	*eap;
{
    u_undo(1);
}

/*
 * ":redo".
 */
/*ARGSUSED*/
    static void
ex_redo(eap)
    exarg_T	*eap;
{
    u_redo(1);
}

/*
 * ":redir": start/stop redirection.
 */
    static void
ex_redir(eap)
    exarg_T	*eap;
{
    char	*mode;
    char_u	*fname;

    if (STRICMP(eap->arg, "END") == 0)
	close_redir();
    else
    {
	if (*eap->arg == '>')
	{
	    ++eap->arg;
	    if (*eap->arg == '>')
	    {
		++eap->arg;
		mode = "a";
	    }
	    else
		mode = "w";
	    eap->arg = skipwhite(eap->arg);

	    close_redir();

	    /* Expand environment variables and "~/". */
	    fname = expand_env_save(eap->arg);
	    if (fname == NULL)
		return;
#ifdef FEAT_BROWSE
	    if (cmdmod.browse)
	    {
		char_u	*browseFile;

		browseFile = do_browse(TRUE, (char_u *)_("Save Redirection"),
		       fname, NULL, NULL, BROWSE_FILTER_ALL_FILES, curbuf);
		if (browseFile == NULL)
		    return;		/* operation cancelled */
		vim_free(fname);
		fname = browseFile;
		eap->forceit = TRUE;	/* since dialog already asked */
	    }
#endif

	    redir_fd = open_exfile(fname, eap->forceit, mode);
	    vim_free(fname);
	}
#ifdef FEAT_EVAL
	else if (*eap->arg == '@')
	{
	    /* redirect to a register a-z (resp. A-Z for appending) */
	    close_redir();
	    ++eap->arg;
	    if (ASCII_ISALPHA(*eap->arg)
# ifdef FEAT_CLIPBOARD
		    || *eap->arg == '*'
# endif
		    || *eap->arg == '"')
	    {
		redir_reg = *eap->arg;
		if (islower(redir_reg)
# ifdef FEAT_CLIPBOARD
			|| redir_reg == '*'
# endif
			|| redir_reg == '"')
		{
		    /* make register empty */
		    write_reg_contents(redir_reg, (char_u *)"", -1, FALSE);
		}
	    }
	    else
		EMSG(_(e_invarg));
	}
#endif

	/* TODO: redirect to a buffer */

	/* TODO: redirect to an internal variable */

	else
	    EMSG(_(e_invarg));
    }
}

/*
 * ":redraw": force redraw
 */
    static void
ex_redraw(eap)
    exarg_T	*eap;
{
    int		r = RedrawingDisabled;
    int		p = p_lz;

    RedrawingDisabled = 0;
    p_lz = FALSE;
    update_screen(eap->forceit ? CLEAR :
#ifdef FEAT_VISUAL
	    VIsual_active ? INVERTED :
#endif
	    0);
    RedrawingDisabled = r;
    p_lz = p;
    out_flush();
}

    static void
close_redir()
{
    if (redir_fd != NULL)
    {
	fclose(redir_fd);
	redir_fd = NULL;
    }
#ifdef FEAT_EVAL
    redir_reg = 0;
#endif
}

#if defined(FEAT_SESSION) && defined(USE_CRNL)
# define MKSESSION_NL
static int mksession_nl = FALSE;    /* use NL only in put_eol() */
#endif

/*
 * ":mkexrc", ":mkvimrc", ":mkview" and ":mksession".
 */
    static void
ex_mkrc(eap)
    exarg_T	*eap;
{
    FILE	*fd;
    int		failed = FALSE;
    char_u	*fname;
#ifdef FEAT_BROWSE
    char_u	*browseFile = NULL;
#endif
#ifdef FEAT_SESSION
    int		view_session = FALSE;
    int		using_vdir = FALSE;	/* using 'viewdir'? */
    char_u	*viewFile = NULL;
    unsigned	*flagp;
#endif

    if (eap->cmdidx == CMD_mksession || eap->cmdidx == CMD_mkview)
    {
#ifdef FEAT_SESSION
	view_session = TRUE;
#else
	ex_ni(eap);
	return;
#endif
    }

#ifdef FEAT_SESSION
    /* ":mkview" or ":mkview 9": generate file name with 'viewdir' */
    if (eap->cmdidx == CMD_mkview
	    && (*eap->arg == NUL
		|| (isdigit(*eap->arg) && eap->arg[1] == NUL)))
    {
	eap->forceit = TRUE;
	fname = get_view_file(*eap->arg);
	if (fname == NULL)
	    return;
	viewFile = fname;
	using_vdir = TRUE;
    }
    else
#endif
	if (*eap->arg != NUL)
	fname = eap->arg;
    else if (eap->cmdidx == CMD_mkvimrc)
	fname = (char_u *)VIMRC_FILE;
#ifdef FEAT_SESSION
    else if (eap->cmdidx == CMD_mksession)
	fname = (char_u *)SESSION_FILE;
#endif
    else
	fname = (char_u *)EXRC_FILE;

#ifdef FEAT_BROWSE
    if (cmdmod.browse)
    {
	browseFile = do_browse(TRUE,
# ifdef FEAT_SESSION
		eap->cmdidx == CMD_mkview ? (char_u *)_("Save View") :
		eap->cmdidx == CMD_mksession ? (char_u *)_("Save Session") :
# endif
		(char_u *)_("Save Setup"),
		fname, (char_u *)"vim", NULL, BROWSE_FILTER_MACROS, NULL);
	if (browseFile == NULL)
	    goto theend;
	fname = browseFile;
	eap->forceit = TRUE;	/* since dialog already asked */
    }
#endif

#if defined(FEAT_SESSION) && defined(vim_mkdir)
    /* When using 'viewdir' may have to create the directory. */
    if (using_vdir && !mch_isdir(p_vdir))
	vim_mkdir(p_vdir, 0755); /* ignore errors, open_exfile() will fail */
#endif

    fd = open_exfile(fname, eap->forceit, WRITEBIN);
    if (fd != NULL)
    {
#ifdef FEAT_SESSION
	if (eap->cmdidx == CMD_mkview)
	    flagp = &vop_flags;
	else
	    flagp = &ssop_flags;
#endif

#ifdef MKSESSION_NL
	/* "unix" in 'sessionoptions': use NL line separator */
	if (view_session && (*flagp & SSOP_UNIX))
	    mksession_nl = TRUE;
#endif

	/* Write the version command for :mkvimrc */
	if (eap->cmdidx == CMD_mkvimrc)
	    (void)put_line(fd, "version 6.0");

#ifdef FEAT_SESSION
	if (eap->cmdidx != CMD_mkview)
#endif
	{
	    /* Write setting 'compatible' first, because it has side effects */
	    if (p_cp)
		(void)put_line(fd, "set compatible");
	    else
		(void)put_line(fd, "set nocompatible");
	}

#ifdef FEAT_SESSION
	if (!view_session
		|| (eap->cmdidx == CMD_mksession
		    && (*flagp & SSOP_OPTIONS)))
#endif
	    failed |= (makemap(fd, NULL) == FAIL
				   || makeset(fd, OPT_GLOBAL, FALSE) == FAIL);

#ifdef FEAT_SESSION
	if (!failed && view_session)
	{
	    if (put_line(fd, "let s:so_save = &so | let s:siso_save = &siso | set so=0 siso=0") == FAIL)
		failed = TRUE;
	    if (eap->cmdidx == CMD_mksession)
	    {
		char_u dirnow[MAXPATHL];	/* current directory */

		/*
		 * Change to session file's dir.
		 */
		if (mch_dirname(dirnow, MAXPATHL) == FAIL
					    || mch_chdir((char *)dirnow) != 0)
		    *dirnow = NUL;
		if (*dirnow != NUL && (ssop_flags & SSOP_SESDIR))
		{
		    if (vim_chdirfile(fname) == OK)
			shorten_fnames(TRUE);
		}
		else if (*dirnow != NUL
			&& (ssop_flags & SSOP_CURDIR) && globaldir != NULL)
		{
		    (void)mch_chdir((char *)globaldir);
		    shorten_fnames(TRUE);
		}

		failed |= (makeopens(fd, dirnow) == FAIL);

		/* restore original dir */
		if (*dirnow != NUL && ((ssop_flags & SSOP_SESDIR)
			|| ((ssop_flags & SSOP_CURDIR) && globaldir != NULL)))
		{
		    if (mch_chdir((char *)dirnow) != 0)
			EMSG(_(e_prev_dir));
		    shorten_fnames(TRUE);
		}
	    }
	    else
	    {
		failed |= (put_view(fd, curwin, !using_vdir, flagp) == FAIL);
	    }
	    if (put_line(fd, "let &so = s:so_save | let &siso = s:siso_save")
								      == FAIL)
		failed = TRUE;
	}
#endif
	failed |= fclose(fd);

	if (failed)
	    EMSG(_(e_write));
#if defined(FEAT_EVAL) && defined(FEAT_SESSION)
	else if (eap->cmdidx == CMD_mksession)
	{
	    /* successful session write - set this_session var */
	    char_u	tbuf[MAXPATHL];

	    if (vim_FullName(fname, tbuf, MAXPATHL, FALSE) == OK)
		set_vim_var_string(VV_THIS_SESSION, tbuf, -1);
	}
#endif
#ifdef MKSESSION_NL
	mksession_nl = FALSE;
#endif
    }

#ifdef FEAT_BROWSE
theend:
    vim_free(browseFile);
#endif
#ifdef FEAT_SESSION
    vim_free(viewFile);
#endif
}

/*
 * Open a file for writing for an Ex command, with some checks.
 * Return file descriptor, or NULL on failure.
 */
    FILE *
open_exfile(fname, forceit, mode)
    char_u	*fname;
    int		forceit;
    char	*mode;	    /* "w" for create new file or "a" for append */
{
    FILE	*fd;

#ifdef UNIX
    /* with Unix it is possible to open a directory */
    if (mch_isdir(fname))
    {
	EMSG2(_(e_isadir2), fname);
	return NULL;
    }
#endif
    if (!forceit && *mode != 'a' && vim_fexists(fname))
    {
	EMSG2(_("E189: \"%s\" exists (add ! to override)"), fname);
	return NULL;
    }

    if ((fd = mch_fopen((char *)fname, mode)) == NULL)
	EMSG2(_("E190: Cannot open \"%s\" for writing"), fname);

    return fd;
}

/*
 * ":mark" and ":k".
 */
    static void
ex_mark(eap)
    exarg_T	*eap;
{
    pos_T	pos;

    if (*eap->arg == NUL)		/* No argument? */
	EMSG(_(e_argreq));
    else if (eap->arg[1] != NUL)	/* more than one character? */
	EMSG(_(e_trailing));
    else
    {
	pos = curwin->w_cursor;		/* save curwin->w_cursor */
	curwin->w_cursor.lnum = eap->line2;
	beginline(BL_WHITE | BL_FIX);
	if (setmark(*eap->arg) == FAIL)	/* set mark */
	    EMSG(_("E191: Argument must be a letter or forward/backward quote"));
	curwin->w_cursor = pos;		/* restore curwin->w_cursor */
    }
}

/*
 * Update w_topline, w_leftcol and the cursor position.
 */
    void
update_topline_cursor()
{
    check_cursor();		/* put cursor on valid line */
    update_topline();
    if (!curwin->w_p_wrap)
	validate_cursor();
    update_curswant();
}

#ifdef FEAT_EX_EXTRA
/*
 * ":normal[!] {commands}": Execute normal mode commands.
 */
    static void
ex_normal(eap)
    exarg_T	*eap;
{
    oparg_T	oa;
    int		save_msg_scroll = msg_scroll;
    int		save_restart_edit = restart_edit;
    int		save_msg_didout = msg_didout;
    int		save_State = State;
    tasave_T	tabuf;
    int		save_insertmode = p_im;
    int		save_finish_op = finish_op;
#ifdef FEAT_MBYTE
    char_u	*arg = NULL;
    int		l;
    char_u	*p;
#endif

    if (ex_normal_busy >= p_mmd)
    {
	EMSG(_("E192: Recursive use of :normal too deep"));
	return;
    }
    ++ex_normal_busy;

    msg_scroll = FALSE;	    /* no msg scrolling in Normal mode */
    restart_edit = 0;	    /* don't go to Insert mode */
    p_im = FALSE;	    /* don't use 'insertmode' */

#ifdef FEAT_MBYTE
    /*
     * vgetc() expects a CSI and K_SPECIAL to have been escaped.  Don't do
     * this for the K_SPECIAL leading byte, otherwise special keys will not
     * work.
     */
    if (has_mbyte)
    {
	int	len = 0;

	/* Count the number of characters to be escaped. */
	for (p = eap->arg; *p != NUL; ++p)
	{
# ifdef FEAT_GUI
	    if (*p == CSI)  /* leadbyte CSI */
		len += 2;
# endif
	    for (l = (*mb_ptr2len_check)(p) - 1; l > 0; --l)
		if (*++p == K_SPECIAL	  /* trailbyte K_SPECIAL or CSI */
# ifdef FEAT_GUI
			|| *p == CSI
# endif
			)
		    len += 2;
	}
	if (len > 0)
	{
	    arg = alloc((unsigned)(STRLEN(eap->arg) + len + 1));
	    if (arg != NULL)
	    {
		len = 0;
		for (p = eap->arg; *p != NUL; ++p)
		{
		    arg[len++] = *p;
# ifdef FEAT_GUI
		    if (*p == CSI)
		    {
			arg[len++] = KS_EXTRA;
			arg[len++] = (int)KE_CSI;
		    }
# endif
		    for (l = (*mb_ptr2len_check)(p) - 1; l > 0; --l)
		    {
			arg[len++] = *++p;
			if (*p == K_SPECIAL)
			{
			    arg[len++] = KS_SPECIAL;
			    arg[len++] = KE_FILLER;
			}
# ifdef FEAT_GUI
			else if (*p == CSI)
			{
			    arg[len++] = KS_EXTRA;
			    arg[len++] = (int)KE_CSI;
			}
# endif
		    }
		    arg[len] = NUL;
		}
	    }
	}
    }
#endif

    /*
     * Save the current typeahead.  This is required to allow using ":normal"
     * from an event handler and makes sure we don't hang when the argument
     * ends with half a command.
     */
    save_typeahead(&tabuf);
    if (tabuf.typebuf_valid)
    {
	/*
	 * Repeat the :normal command for each line in the range.  When no
	 * range given, execute it just once, without positioning the cursor
	 * first.
	 */
	do
	{
	    clear_oparg(&oa);
	    finish_op = FALSE;
	    if (eap->addr_count != 0)
	    {
		curwin->w_cursor.lnum = eap->line1++;
		curwin->w_cursor.col = 0;
	    }

	    /*
	     * Stuff the argument into the typeahead buffer.
	     * Execute normal_cmd() until there is no typeahead left.
	     */
	    ins_typebuf(
#ifdef FEAT_MBYTE
		    arg != NULL ? arg :
#endif
		    eap->arg, eap->forceit ? REMAP_NONE : REMAP_YES, 0,
								 TRUE, FALSE);
	    while ((!stuff_empty() || (!typebuf_typed() && typebuf.tb_len > 0))
		    && !got_int)
	    {
		update_topline_cursor();
		normal_cmd(&oa, FALSE);	/* execute a Normal mode cmd */
	    }
	}
	while (eap->addr_count > 0 && eap->line1 <= eap->line2 && !got_int);
    }

    /* Might not return to the main loop when in an event handler. */
    update_topline_cursor();

    /* Restore the previous typeahead. */
    restore_typeahead(&tabuf);

    --ex_normal_busy;
    msg_scroll = save_msg_scroll;
    restart_edit = save_restart_edit;
    p_im = save_insertmode;
    finish_op = save_finish_op;
    msg_didout |= save_msg_didout;	/* don't reset msg_didout now */

    /* Restore the state (needed when called from a function executed for
     * 'indentexpr'). */
    State = save_State;
#ifdef FEAT_MBYTE
    vim_free(arg);
#endif
}

/*
 * ":startinsert"
 */
    static void
ex_startinsert(eap)
    exarg_T	*eap;
{
    if (eap->forceit)
    {
	coladvance((colnr_T)MAXCOL);
	curwin->w_curswant = MAXCOL;
	curwin->w_set_curswant = FALSE;
	restart_edit = 'a';
    }
    else
	restart_edit = 'i';
}

/*
 * ":stopinsert"
 */
/*ARGSUSED*/
    static void
ex_stopinsert(eap)
    exarg_T	*eap;
{
    restart_edit = 0;
    stop_insert_mode = TRUE;
}
#endif

#ifdef FEAT_FIND_ID
    static void
ex_checkpath(eap)
    exarg_T	*eap;
{
    find_pattern_in_path(NULL, 0, 0, FALSE, FALSE, CHECK_PATH, 1L,
				   eap->forceit ? ACTION_SHOW_ALL : ACTION_SHOW,
					      (linenr_T)1, (linenr_T)MAXLNUM);
}

#if defined(FEAT_WINDOWS) && defined(FEAT_QUICKFIX)
/*
 * ":psearch"
 */
    static void
ex_psearch(eap)
    exarg_T	*eap;
{
    g_do_tagpreview = p_pvh;
    ex_findpat(eap);
    g_do_tagpreview = 0;
}
#endif

    static void
ex_findpat(eap)
    exarg_T	*eap;
{
    int		whole = TRUE;
    long	n;
    char_u	*p;
    int		action;

    switch (cmdnames[eap->cmdidx].cmd_name[2])
    {
	case 'e':	/* ":psearch", ":isearch" and ":dsearch" */
		if (cmdnames[eap->cmdidx].cmd_name[0] == 'p')
		    action = ACTION_GOTO;
		else
		    action = ACTION_SHOW;
		break;
	case 'i':	/* ":ilist" and ":dlist" */
		action = ACTION_SHOW_ALL;
		break;
	case 'u':	/* ":ijump" and ":djump" */
		action = ACTION_GOTO;
		break;
	default:	/* ":isplit" and ":dsplit" */
		action = ACTION_SPLIT;
		break;
    }

    n = 1;
    if (isdigit(*eap->arg))	/* get count */
    {
	n = getdigits(&eap->arg);
	eap->arg = skipwhite(eap->arg);
    }
    if (*eap->arg == '/')   /* Match regexp, not just whole words */
    {
	whole = FALSE;
	++eap->arg;
	p = skip_regexp(eap->arg, '/', p_magic, NULL);
	if (*p)
	{
	    *p++ = NUL;
	    p = skipwhite(p);

	    /* Check for trailing illegal characters */
	    if (!ends_excmd(*p))
		eap->errmsg = e_trailing;
	    else
		eap->nextcmd = check_nextcmd(p);
	}
    }
    if (!eap->skip)
	find_pattern_in_path(eap->arg, 0, (int)STRLEN(eap->arg),
			    whole, !eap->forceit,
			    *eap->cmd == 'd' ?	FIND_DEFINE : FIND_ANY,
			    n, action, eap->line1, eap->line2);
}
#endif

#ifdef FEAT_WINDOWS

# ifdef FEAT_QUICKFIX
/*
 * ":ptag", ":ptselect", ":ptjump", ":ptnext", etc.
 */
    static void
ex_ptag(eap)
    exarg_T	*eap;
{
    g_do_tagpreview = p_pvh;
    ex_tag_cmd(eap, cmdnames[eap->cmdidx].cmd_name + 1);
}

/*
 * ":pedit"
 */
    static void
ex_pedit(eap)
    exarg_T	*eap;
{
    win_T	*curwin_save = curwin;

    g_do_tagpreview = p_pvh;
    prepare_tagpreview();
    keep_help_flag = curwin_save->w_buffer->b_help;
    do_exedit(eap, NULL);
    keep_help_flag = FALSE;
    if (curwin != curwin_save && win_valid(curwin_save))
    {
	/* Return cursor to where we were */
	validate_cursor();
	redraw_later(VALID);
	win_enter(curwin_save, TRUE);
    }
    g_do_tagpreview = 0;
}
# endif

/*
 * ":stag", ":stselect" and ":stjump".
 */
    static void
ex_stag(eap)
    exarg_T	*eap;
{
    postponed_split = -1;
    ex_tag_cmd(eap, cmdnames[eap->cmdidx].cmd_name + 1);
}
#endif

/*
 * ":tag", ":tselect", ":tjump", ":tnext", etc.
 */
    static void
ex_tag(eap)
    exarg_T	*eap;
{
    ex_tag_cmd(eap, cmdnames[eap->cmdidx].cmd_name);
}

    static void
ex_tag_cmd(eap, name)
    exarg_T	*eap;
    char_u	*name;
{
    int		cmd;

    switch (name[1])
    {
	case 'j': cmd = DT_JUMP;	/* ":tjump" */
		  break;
	case 's': cmd = DT_SELECT;	/* ":tselect" */
		  break;
	case 'p': cmd = DT_PREV;	/* ":tprevious" */
		  break;
	case 'N': cmd = DT_PREV;	/* ":tNext" */
		  break;
	case 'n': cmd = DT_NEXT;	/* ":tnext" */
		  break;
	case 'o': cmd = DT_POP;		/* ":pop" */
		  break;
	case 'f':			/* ":tfirst" */
	case 'r': cmd = DT_FIRST;	/* ":trewind" */
		  break;
	case 'l': cmd = DT_LAST;	/* ":tlast" */
		  break;
	default:			/* ":tag" */
#ifdef FEAT_CSCOPE
		  if (p_cst)
		  {
		      do_cstag(eap);
		      return;
		  }
#endif
		  cmd = DT_TAG;
		  break;
    }

    do_tag(eap->arg, cmd, eap->addr_count > 0 ? (int)eap->line2 : 1,
							  eap->forceit, TRUE);
}

/*
 * Evaluate cmdline variables.
 *
 * change '%'	    to curbuf->b_ffname
 *	  '#'	    to curwin->w_altfile
 *	  '<cword>' to word under the cursor
 *	  '<cWORD>' to WORD under the cursor
 *	  '<cfile>' to path name under the cursor
 *	  '<sfile>' to sourced file name
 *	  '<afile>' to file name for autocommand
 *	  '<abuf>'  to buffer number for autocommand
 *	  '<amatch>' to matching name for autocommand
 *
 * When an error is detected, "errormsg" is set to a non-NULL pointer (may be
 * "" for error without a message) and NULL is returned.
 * Returns an allocated string if a valid match was found.
 * Returns NULL if no match was found.	"usedlen" then still contains the
 * number of characters to skip.
 */
    char_u *
eval_vars(src, usedlen, lnump, errormsg, srcstart)
    char_u	*src;		/* pointer into commandline */
    int		*usedlen;	/* characters after src that are used */
    linenr_T	*lnump;		/* line number for :e command, or NULL */
    char_u	**errormsg;	/* pointer to error message */
    char_u	*srcstart;	/* beginning of valid memory for src */
{
    int		i;
    char_u	*s;
    char_u	*result;
    char_u	*resultbuf = NULL;
    int		resultlen;
    buf_T	*buf;
    int		valid = VALID_HEAD + VALID_PATH;    /* assume valid result */
    int		spec_idx;
#ifdef FEAT_MODIFY_FNAME
    int		skip_mod = FALSE;
#endif
    static char *(spec_str[]) =
	{
		    "%",
#define SPEC_PERC   0
		    "#",
#define SPEC_HASH   1
		    "<cword>",		/* cursor word */
#define SPEC_CWORD  2
		    "<cWORD>",		/* cursor WORD */
#define SPEC_CCWORD 3
		    "<cfile>",		/* cursor path name */
#define SPEC_CFILE  4
		    "<sfile>",		/* ":so" file name */
#define SPEC_SFILE  5
#ifdef FEAT_AUTOCMD
		    "<afile>",		/* autocommand file name */
# define SPEC_AFILE 6
		    "<abuf>",		/* autocommand buffer number */
# define SPEC_ABUF  7
		    "<amatch>",		/* autocommand match name */
# define SPEC_AMATCH 8
#endif
#ifdef FEAT_CLIENTSERVER
		    "<client>"
# define SPEC_CLIENT 9
#endif
		};
#define SPEC_COUNT  (sizeof(spec_str) / sizeof(char *))

#if defined(FEAT_AUTOCMD) || defined(FEAT_CLIENTSERVER)
    char_u	strbuf[30];
#endif

    *errormsg = NULL;

    /*
     * Check if there is something to do.
     */
    for (spec_idx = 0; spec_idx < SPEC_COUNT; ++spec_idx)
    {
	*usedlen = (int)STRLEN(spec_str[spec_idx]);
	if (STRNCMP(src, spec_str[spec_idx], *usedlen) == 0)
	    break;
    }
    if (spec_idx == SPEC_COUNT)	    /* no match */
    {
	*usedlen = 1;
	return NULL;
    }

    /*
     * Skip when preceded with a backslash "\%" and "\#".
     * Note: In "\\%" the % is also not recognized!
     */
    if (src > srcstart && src[-1] == '\\')
    {
	*usedlen = 0;
	STRCPY(src - 1, src);		/* remove backslash */
	return NULL;
    }

    /*
     * word or WORD under cursor
     */
    if (spec_idx == SPEC_CWORD || spec_idx == SPEC_CCWORD)
    {
	resultlen = find_ident_under_cursor(&result, spec_idx == SPEC_CWORD ?
				      (FIND_IDENT|FIND_STRING) : FIND_STRING);
	if (resultlen == 0)
	{
	    *errormsg = (char_u *)"";
	    return NULL;
	}
    }

    /*
     * '#': Alternate file name
     * '%': Current file name
     *	    File name under the cursor
     *	    File name for autocommand
     *	and following modifiers
     */
    else
    {
	switch (spec_idx)
	{
	case SPEC_PERC:		/* '%': current file */
		if (curbuf->b_fname == NULL)
		{
		    result = (char_u *)"";
		    valid = 0;	    /* Must have ":p:h" to be valid */
		}
		else
#ifdef RISCOS
		    /* Always use the full path for RISC OS if possible. */
		    result = curbuf->b_ffname;
		    if (result == NULL)
			result = curbuf->b_fname;
#else
		    result = curbuf->b_fname;
#endif
		break;

	case SPEC_HASH:		/* '#' or "#99": alternate file */
		if (src[1] == '#')  /* "##": the argument list */
		{
		    result = arg_all();
		    resultbuf = result;
		    *usedlen = 2;
#ifdef FEAT_MODIFY_FNAME
		    skip_mod = TRUE;
#endif
		    break;
		}
		s = src + 1;
		i = (int)getdigits(&s);
		*usedlen = (int)(s - src); /* length of what we expand */

		buf = buflist_findnr(i);
		if (buf == NULL)
		{
		    *errormsg = (char_u *)_("E194: No alternate file name to substitute for '#'");
		    return NULL;
		}
		if (lnump != NULL)
		    *lnump = ECMD_LAST;
		if (buf->b_fname == NULL)
		{
		    result = (char_u *)"";
		    valid = 0;	    /* Must have ":p:h" to be valid */
		}
		else
		    result = buf->b_fname;
		break;

#ifdef FEAT_SEARCHPATH
	case SPEC_CFILE:	/* file name under cursor */
		result = file_name_at_cursor(FNAME_MESS|FNAME_HYP, 1L);
		if (result == NULL)
		{
		    *errormsg = (char_u *)"";
		    return NULL;
		}
		resultbuf = result;	    /* remember allocated string */
		break;
#endif

#ifdef FEAT_AUTOCMD
	case SPEC_AFILE:	/* file name for autocommand */
		result = autocmd_fname;
		if (result == NULL)
		{
		    *errormsg = (char_u *)_("E495: no autocommand file name to substitute for \"<afile>\"");
		    return NULL;
		}
		break;

	case SPEC_ABUF:		/* buffer number for autocommand */
		if (autocmd_bufnr <= 0)
		{
		    *errormsg = (char_u *)_("E496: no autocommand buffer number to substitute for \"<abuf>\"");
		    return NULL;
		}
		sprintf((char *)strbuf, "%d", autocmd_bufnr);
		result = strbuf;
		break;

	case SPEC_AMATCH:	/* match name for autocommand */
		result = autocmd_match;
		if (result == NULL)
		{
		    *errormsg = (char_u *)_("E497: no autocommand match name to substitute for \"<amatch>\"");
		    return NULL;
		}
		break;

#endif
	case SPEC_SFILE:	/* file name for ":so" command */
		result = sourcing_name;
		if (result == NULL)
		{
		    *errormsg = (char_u *)_("E498: no :source file name to substitute for \"<sfile>\"");
		    return NULL;
		}
		break;
#if defined(FEAT_CLIENTSERVER)
	case SPEC_CLIENT:	/* Source of last submitted input */
		sprintf((char *)strbuf, "0x%x", (unsigned int)clientWindow);
		result = strbuf;
		break;
#endif
	}

	resultlen = (int)STRLEN(result);	/* length of new string */
	if (src[*usedlen] == '<')	/* remove the file name extension */
	{
	    ++*usedlen;
#ifdef RISCOS
	    if ((s = vim_strrchr(result, '/')) != NULL && s >= gettail(result))
#else
	    if ((s = vim_strrchr(result, '.')) != NULL && s >= gettail(result))
#endif
		resultlen = (int)(s - result);
	}
#ifdef FEAT_MODIFY_FNAME
	else if (!skip_mod)
	{
	    valid |= modify_fname(src, usedlen, &result, &resultbuf,
								  &resultlen);
	    if (result == NULL)
	    {
		*errormsg = (char_u *)"";
		return NULL;
	    }
	}
#endif
    }

    if (resultlen == 0 || valid != VALID_HEAD + VALID_PATH)
    {
	if (valid != VALID_HEAD + VALID_PATH)
	    /* xgettext:no-c-format */
	    *errormsg = (char_u *)_("E499: Empty file name for '%' or '#', only works with \":p:h\"");
	else
	    *errormsg = (char_u *)_("E500: Evaluates to an empty string");
	result = NULL;
    }
    else
	result = vim_strnsave(result, resultlen);
    vim_free(resultbuf);
    return result;
}

/*
 * Concatenate all files in the argument list, separated by spaces, and return
 * it in one allocated string.
 * Spaces and backslashes in the file names are escaped with a backslash.
 * Returns NULL when out of memory.
 */
    static char_u *
arg_all()
{
    int		len;
    int		idx;
    char_u	*retval = NULL;
    char_u	*p;

    /*
     * Do this loop two times:
     * first time: compute the total length
     * second time: concatenate the names
     */
    for (;;)
    {
	len = 0;
	for (idx = 0; idx < ARGCOUNT; ++idx)
	{
	    p = alist_name(&ARGLIST[idx]);
	    if (p != NULL)
	    {
		if (len > 0)
		{
		    /* insert a space in between names */
		    if (retval != NULL)
			retval[len] = ' ';
		    ++len;
		}
		for ( ; *p != NUL; ++p)
		{
		    if (*p == ' ' || *p == '\\')
		    {
			/* insert a backslash */
			if (retval != NULL)
			    retval[len] = '\\';
			++len;
		    }
		    if (retval != NULL)
			retval[len] = *p;
		    ++len;
		}
	    }
	}

	/* second time: break here */
	if (retval != NULL)
	{
	    retval[len] = NUL;
	    break;
	}

	/* allocate memory */
	retval = alloc(len + 1);
	if (retval == NULL)
	    break;
    }

    return retval;
}

#if defined(FEAT_AUTOCMD) || defined(PROTO)
/*
 * Expand the <sfile> string in "arg".
 *
 * Returns an allocated string, or NULL for any error.
 */
    char_u *
expand_sfile(arg)
    char_u	*arg;
{
    char_u	*errormsg;
    int		len;
    char_u	*result;
    char_u	*newres;
    char_u	*repl;
    int		srclen;
    char_u	*p;

    result = vim_strsave(arg);
    if (result == NULL)
	return NULL;

    for (p = result; *p; )
    {
	if (STRNCMP(p, "<sfile>", 7) != 0)
	    ++p;
	else
	{
	    /* replace "<sfile>" with the sourced file name, and do ":" stuff */
	    repl = eval_vars(p, &srclen, NULL, &errormsg, result);
	    if (errormsg != NULL)
	    {
		if (*errormsg)
		    emsg(errormsg);
		vim_free(result);
		return NULL;
	    }
	    if (repl == NULL)		/* no match (cannot happen) */
	    {
		p += srclen;
		continue;
	    }
	    len = (int)STRLEN(result) - srclen + (int)STRLEN(repl) + 1;
	    newres = alloc(len);
	    if (newres == NULL)
	    {
		vim_free(repl);
		vim_free(result);
		return NULL;
	    }
	    mch_memmove(newres, result, (size_t)(p - result));
	    STRCPY(newres + (p - result), repl);
	    len = (int)STRLEN(newres);
	    STRCAT(newres, p + srclen);
	    vim_free(repl);
	    vim_free(result);
	    result = newres;
	    p = newres + len;		/* continue after the match */
	}
    }

    return result;
}
#endif

#ifdef FEAT_SESSION
static int ses_win_rec __ARGS((FILE *fd, frame_T *fr));
static frame_T *ses_skipframe __ARGS((frame_T *fr));
static int ses_do_frame __ARGS((frame_T *fr));
static int ses_do_win __ARGS((win_T *wp));
static int ses_arglist __ARGS((FILE *fd, char *cmd, garray_T *gap, int fullname, unsigned *flagp));
static int ses_put_fname __ARGS((FILE *fd, char_u *name, unsigned *flagp));
static int ses_fname __ARGS((FILE *fd, buf_T *buf, unsigned *flagp));

/*
 * Write openfile commands for the current buffers to an .exrc file.
 * Return FAIL on error, OK otherwise.
 */
    static int
makeopens(fd, dirnow)
    FILE	*fd;
    char_u	*dirnow;	/* Current directory name */
{
    buf_T	*buf;
    int		only_save_windows = TRUE;
    int		nr;
    int		cnr = 1;
    int		restore_size = TRUE;
    win_T	*wp;
    char_u	*sname;

    if (ssop_flags & SSOP_BUFFERS)
	only_save_windows = FALSE;		/* Save ALL buffers */

    /*
     * Begin by setting the this_session variable, and then other
     * sessionable variables.
     */
#ifdef FEAT_EVAL
    if (put_line(fd, "let v:this_session=expand(\"<sfile>:p\")") == FAIL)
	return FAIL;
    if (ssop_flags & SSOP_GLOBALS)
	if (store_session_globals(fd) == FAIL)
	    return FAIL;
#endif

    /*
     * Close all windows but one.
     */
    if (put_line(fd, "silent only") == FAIL)
	return FAIL;

    /*
     * Now a :cd command to the session directory or the current directory
     */
    if (ssop_flags & SSOP_SESDIR)
    {
	if (put_line(fd, "exe \"cd \" . expand(\"<sfile>:p:h\")") == FAIL)
	    return FAIL;
    }
    else if (ssop_flags & SSOP_CURDIR)
    {
	sname = home_replace_save(NULL, globaldir != NULL ? globaldir : dirnow);
	if (sname == NULL
		|| fprintf(fd, "cd %s", sname) < 0 || put_eol(fd) == FAIL)
	    return FAIL;
	vim_free(sname);
    }

    /*
     * Now save the current files, current buffer first.
     */
    if (put_line(fd, "set shortmess=aoO") == FAIL)
	return FAIL;

    /* Now put the other buffers into the buffer list */
    for (buf = firstbuf; buf != NULL; buf = buf->b_next)
    {
	if (!(only_save_windows && buf->b_nwindows == 0)
		&& !(buf->b_help && !(ssop_flags & SSOP_HELP))
		&& buf->b_fname != NULL
		&& buf->b_p_bl)
	{
	    if (fprintf(fd, "badd +%ld ", buf->b_wininfo == NULL ? 1L
					   : buf->b_wininfo->wi_fpos.lnum) < 0
		    || ses_fname(fd, buf, &ssop_flags) == FAIL)
		return FAIL;
	}
    }

    /* the global argument list */
    if (ses_arglist(fd, "args", &global_alist.al_ga,
			    !(ssop_flags & SSOP_CURDIR), &ssop_flags) == FAIL)
	return FAIL;

    if (ssop_flags & SSOP_RESIZE)
    {
	/* Note: after the restore we still check it worked!*/
	if (fprintf(fd, "set lines=%ld columns=%ld" , Rows, Columns) < 0
		|| put_eol(fd) == FAIL)
	    return FAIL;
    }

#ifdef FEAT_GUI
    if (gui.in_use && (ssop_flags & SSOP_WINPOS))
    {
	int	x, y;

	if (gui_mch_get_winpos(&x, &y) == OK)
	{
	    /* Note: after the restore we still check it worked!*/
	    if (fprintf(fd, "winpos %d %d", x, y) < 0 || put_eol(fd) == FAIL)
		return FAIL;
	}
    }
#endif

    /*
     * Save current window layout.
     */
    if (put_line(fd, "set splitbelow splitright") == FAIL)
	return FAIL;
    if (ses_win_rec(fd, topframe) == FAIL)
	return FAIL;
    if (!p_sb && put_line(fd, "set nosplitbelow") == FAIL)
	return FAIL;
    if (!p_spr && put_line(fd, "set nosplitright") == FAIL)
	return FAIL;

    /*
     * Check if window sizes can be restored (no windows omitted).
     * Remember the window number of the current window after restoring.
     */
    nr = 0;
    for (wp = firstwin; wp != NULL; wp = W_NEXT(wp))
    {
	if (ses_do_win(wp))
	    ++nr;
	else
	    restore_size = FALSE;
	if (curwin == wp)
	    cnr = nr;
    }

    /* Go to the first window. */
    if (put_line(fd, "wincmd t") == FAIL)
	return FAIL;

    /*
     * If more than one window, see if sizes can be restored.
     * First set 'winheight' and 'winwidth' to 1 to avoid the windows being
     * resized when moving between windows.
     */
    if (put_line(fd, "set winheight=1 winwidth=1") == FAIL)
	return FAIL;
    if (nr > 1)
    {
	if (restore_size && (ssop_flags & SSOP_WINSIZE))
	{
	    for (wp = firstwin; wp != NULL; wp = wp->w_next)
	    {
		if (!ses_do_win(wp))
		    continue;

		/* restore height when not full height */
		if (wp->w_height + wp->w_status_height < topframe->fr_height
			&& (fprintf(fd,
				"exe 'resize ' . ((&lines * %ld + %ld) / %ld)",
				(long)wp->w_height, Rows / 2, Rows) < 0
						      || put_eol(fd) == FAIL))
		    return FAIL;

		/* restore width when not full width */
		if (wp->w_width < Columns && (fprintf(fd,
			"exe 'vert resize ' . ((&columns * %ld + %ld) / %ld)",
				(long)wp->w_width, Columns / 2, Columns) < 0
						      || put_eol(fd) == FAIL))
		    return FAIL;
		if (put_line(fd, "wincmd w") == FAIL)
		    return FAIL;

	    }
	}
	else
	{
	    /* Just equalise window sizes */
	    if (put_line(fd, "wincmd =") == FAIL)
		return FAIL;
	}
    }

    /*
     * Restore the view of the window (options, file, cursor, etc.).
     */
    for (wp = firstwin; wp != NULL; wp = wp->w_next)
    {
	if (!ses_do_win(wp))
	    continue;
	if (put_view(fd, wp, TRUE, &ssop_flags) == FAIL)
	    return FAIL;
	if (nr > 1 && put_line(fd, "wincmd w") == FAIL)
	    return FAIL;
    }

    /*
     * Restore cursor to the current window if it's not the first one.
     */
    if (cnr > 1 && (fprintf(fd, "%dwincmd w", cnr) < 0
						      || put_eol(fd) == FAIL))
	return FAIL;

    /* Re-apply 'winheight', 'winwidth' and 'shortmess'. */
    if (fprintf(fd, "set winheight=%ld winwidth=%ld shortmess=%s",
			       p_wh, p_wiw, p_shm) < 0 || put_eol(fd) == FAIL)
	return FAIL;

    /*
     * Lastly, execute the x.vim file if it exists.
     */
    if (put_line(fd, "let s:sx = expand(\"<sfile>:p:r\").\"x.vim\"") == FAIL
	    || put_line(fd, "if file_readable(s:sx)") == FAIL
	    || put_line(fd, "  exe \"source \" . s:sx") == FAIL
	    || put_line(fd, "endif") == FAIL)
	return FAIL;

    return OK;
}

/*
 * Write commands to "fd" to recursively create windows for frame "fr",
 * horizontally and vertically split.
 * After the commands the last window in the frame is the current window.
 * Returns FAIL when writing the commands to "fd" fails.
 */
    static int
ses_win_rec(fd, fr)
    FILE	*fd;
    frame_T	*fr;
{
    frame_T	*frc;
    int		count = 0;

    if (fr->fr_layout != FR_LEAF)
    {
	/* Find first frame that's not skipped and then create a window for
	 * each following one (first frame is already there). */
	frc = ses_skipframe(fr->fr_child);
	if (frc != NULL)
	    while ((frc = ses_skipframe(frc->fr_next)) != NULL)
	    {
		/* Make window as big as possible so that we have lots of room
		 * to split. */
		if (put_line(fd, "wincmd _ | wincmd |") == FAIL
			|| put_line(fd, fr->fr_layout == FR_COL
						? "split" : "vsplit") == FAIL)
		    return FAIL;
		++count;
	    }

	/* Go back to the first window. */
	if (count > 0 && (fprintf(fd, fr->fr_layout == FR_COL
			? "%dwincmd k" : "%dwincmd h", count) < 0
						      || put_eol(fd) == FAIL))
	    return FAIL;

	/* Recursively create frames/windows in each window of this column or
	 * row. */
	frc = ses_skipframe(fr->fr_child);
	while (frc != NULL)
	{
	    ses_win_rec(fd, frc);
	    frc = ses_skipframe(frc->fr_next);
	    /* Go to next window. */
	    if (frc != NULL && put_line(fd, "wincmd w") == FAIL)
		return FAIL;
	}
    }
    return OK;
}

/*
 * Skip frames that don't contain windows we want to save in the Session.
 * Returns NULL when there none.
 */
    static frame_T *
ses_skipframe(fr)
    frame_T	*fr;
{
    frame_T	*frc;

    for (frc = fr; frc != NULL; frc = frc->fr_next)
	if (ses_do_frame(frc))
	    break;
    return frc;
}

/*
 * Return TRUE if frame "fr" has a window somewhere that we want to save in
 * the Session.
 */
    static int
ses_do_frame(fr)
    frame_T	*fr;
{
    frame_T	*frc;

    if (fr->fr_layout == FR_LEAF)
	return ses_do_win(fr->fr_win);
    for (frc = fr->fr_child; frc != NULL; frc = frc->fr_next)
	if (ses_do_frame(frc))
	    return TRUE;
    return FALSE;
}

/*
 * Return non-zero if window "wp" is to be stored in the Session.
 */
    static int
ses_do_win(wp)
    win_T	*wp;
{
    if (wp->w_buffer->b_fname == NULL
#ifdef FEAT_QUICKFIX
	    /* When 'buftype' is "nofile" can't restore the window contents. */
	    || bt_nofile(wp->w_buffer)
#endif
       )
	return (ssop_flags & SSOP_BLANK);
    if (wp->w_buffer->b_help)
	return (ssop_flags & SSOP_HELP);
    return TRUE;
}

/*
 * Write commands to "fd" to restore the view of a window.
 * Caller must make sure 'scrolloff' is zero.
 */
    static int
put_view(fd, wp, add_edit, flagp)
    FILE	*fd;
    win_T	*wp;
    int		add_edit;	/* add ":edit" command to view */
    unsigned	*flagp;		/* vop_flags or ssop_flags */
{
    win_T	*save_curwin;
    int		f;
    int		do_cursor;

    /* Always restore cursor position for ":mksession".  For ":mkview" only
     * when 'viewoptions' contains "cursor". */
    do_cursor = (flagp == &ssop_flags || *flagp & SSOP_CURSOR);

    /*
     * Local argument list.
     */
    if (wp->w_alist == &global_alist)
    {
	if (put_line(fd, "argglobal") == FAIL)
	    return FAIL;
    }
    else
    {
	if (ses_arglist(fd, "arglocal", &wp->w_alist->al_ga,
			flagp == &vop_flags
			|| !(*flagp & SSOP_CURDIR)
			|| wp->w_localdir != NULL, flagp) == FAIL)
	    return FAIL;
    }

    /* Only when part of a session: restore the argument index. */
    if (wp->w_arg_idx != 0 && flagp == &ssop_flags)
    {
	if (fprintf(fd, "%ldnext", (long)wp->w_arg_idx) < 0
		|| put_eol(fd) == FAIL)
	    return FAIL;
    }

    if (add_edit)
    {
	/*
	 * Load the file.
	 */
	if (wp->w_buffer->b_ffname != NULL
#ifdef FEAT_QUICKFIX
		&& !bt_nofile(wp->w_buffer)
#endif
		)
	{
	    /*
	     * Editing a file in this buffer: use ":edit file".
	     * This may have side effects! (e.g., compressed or network file).
	     */
	    if (fputs("edit ", fd) < 0
		    || ses_fname(fd, wp->w_buffer, flagp) == FAIL)
		return FAIL;
	}
	else
	{
	    /* No file in this buffer, just make it empty. */
	    if (put_line(fd, "enew") == FAIL)
		return FAIL;
#ifdef FEAT_QUICKFIX
	    if (wp->w_buffer->b_ffname != NULL)
	    {
		/* The buffer does have a name, but it's not a file name. */
		if (fputs("file ", fd) < 0
			|| ses_fname(fd, wp->w_buffer, flagp) == FAIL)
		    return FAIL;
	    }
#endif
	    do_cursor = FALSE;
	}
    }

    /*
     * Local mappings and abbreviations.
     */
    if ((*flagp & (SSOP_OPTIONS | SSOP_LOCALOPTIONS))
					 && makemap(fd, wp->w_buffer) == FAIL)
	return FAIL;

    /*
     * Local options.  Need to go to the window temporarily.
     * Store only local values when using ":mkview" and when ":mksession" is
     * used and 'sessionoptions' doesn't include "options".
     * Some folding options are always stored when "folds" is included,
     * otherwise the folds would not be restored correctly.
     */
    save_curwin = curwin;
    curwin = wp;
    curbuf = curwin->w_buffer;
    if (*flagp & (SSOP_OPTIONS | SSOP_LOCALOPTIONS))
	f = makeset(fd, OPT_LOCAL,
			     flagp == &vop_flags || !(*flagp & SSOP_OPTIONS));
#ifdef FEAT_FOLDING
    else if (*flagp & SSOP_FOLDS)
	f = makefoldset(fd);
#endif
    else
	f = OK;
    curwin = save_curwin;
    curbuf = curwin->w_buffer;
    if (f == FAIL)
	return FAIL;

#ifdef FEAT_FOLDING
    /*
     * Save Folds when 'buftype' is empty and for help files.
     */
    if ((*flagp & SSOP_FOLDS)
	    && wp->w_buffer->b_ffname != NULL
	    && (*wp->w_buffer->b_p_bt == NUL || wp->w_buffer->b_help))
    {
	if (put_folds(fd, wp) == FAIL)
	    return FAIL;
    }
#endif

    /*
     * Set the cursor after creating folds, since that moves the cursor.
     */
    if (do_cursor)
    {

	/* Restore the cursor line in the file and relatively in the
	 * window.  Don't use "G", it changes the jumplist. */
	if (fprintf(fd, "let s:l = %ld - ((%ld * winheight(0) + %ld) / %ld)",
		    (long)wp->w_cursor.lnum,
		    (long)(wp->w_cursor.lnum - wp->w_topline),
		    (long)wp->w_height / 2, (long)wp->w_height) < 0
		|| put_eol(fd) == FAIL
		|| put_line(fd, "if s:l < 1 | let s:l = 1 | endif") == FAIL
		|| put_line(fd, "exe s:l") == FAIL
		|| put_line(fd, "normal! zt") == FAIL
		|| fprintf(fd, "%ld", (long)wp->w_cursor.lnum) < 0
		|| put_eol(fd) == FAIL)
	    return FAIL;
	/* Restore the cursor column and left offset when not wrapping. */
	if (wp->w_cursor.col == 0)
	{
	    if (put_line(fd, "normal! 0") == FAIL)
		return FAIL;
	}
	else
	{
	    if (!wp->w_p_wrap && wp->w_leftcol > 0 && wp->w_width > 0)
	    {
		if (fprintf(fd,
			  "let s:c = %ld - ((%ld * winwidth(0) + %ld) / %ld)",
			    (long)wp->w_cursor.col,
			    (long)(wp->w_cursor.col - wp->w_leftcol),
			    (long)wp->w_width / 2, (long)wp->w_width) < 0
			|| put_eol(fd) == FAIL
			|| put_line(fd, "if s:c > 0") == FAIL
			|| fprintf(fd,
			    "  exe 'normal! 0' . s:c . 'lzs' . (%ld - s:c) . 'l'",
			    (long)wp->w_cursor.col) < 0
			|| put_eol(fd) == FAIL
			|| put_line(fd, "else") == FAIL
			|| fprintf(fd, "  normal! 0%dl", wp->w_cursor.col) < 0
			|| put_eol(fd) == FAIL
			|| put_line(fd, "endif") == FAIL)
		    return FAIL;
	    }
	    else
	    {
		if (fprintf(fd, "normal! 0%dl", wp->w_cursor.col) < 0
			|| put_eol(fd) == FAIL)
		    return FAIL;
	    }
	}
    }

    /*
     * Local directory.
     */
    if (wp->w_localdir != NULL)
    {
	if (fputs("lcd ", fd) < 0
		|| ses_put_fname(fd, wp->w_localdir, flagp) == FAIL
		|| put_eol(fd) == FAIL)
	    return FAIL;
    }

    return OK;
}

/*
 * Write an argument list to the session file.
 * Returns FAIL if writing fails.
 */
    static int
ses_arglist(fd, cmd, gap, fullname, flagp)
    FILE	*fd;
    char	*cmd;
    garray_T	*gap;
    int		fullname;	/* TRUE: use full path name */
    unsigned	*flagp;
{
    int		i;
    char_u	buf[MAXPATHL];
    char_u	*s;

    if (gap->ga_len == 0)
	return put_line(fd, "silent! argdel *");
    if (fputs(cmd, fd) < 0)
	return FAIL;
    for (i = 0; i < gap->ga_len; ++i)
    {
	/* NULL file names are skipped (only happens when out of memory). */
	s = alist_name(&((aentry_T *)gap->ga_data)[i]);
	if (s != NULL)
	{
	    if (fullname)
	    {
		(void)vim_FullName(s, buf, MAXPATHL, FALSE);
		s = buf;
	    }
	    if (fputs(" ", fd) < 0 || ses_put_fname(fd, s, flagp) == FAIL)
		return FAIL;
	}
    }
    return put_eol(fd);
}

/*
 * Write a buffer name to the session file.
 * Also ends the line.
 * Returns FAIL if writing fails.
 */
    static int
ses_fname(fd, buf, flagp)
    FILE	*fd;
    buf_T	*buf;
    unsigned	*flagp;
{
    char_u	*name;

    /* Use the short file name if the current directory is known at the time
     * the session file will be sourced.  Don't do this for ":mkview", we
     * don't know the current directory. */
    if (buf->b_sfname != NULL
	    && flagp == &ssop_flags
	    && (ssop_flags & (SSOP_CURDIR | SSOP_SESDIR)))
	name = buf->b_sfname;
    else
	name = buf->b_ffname;
    if (ses_put_fname(fd, name, flagp) == FAIL || put_eol(fd) == FAIL)
	return FAIL;
    return OK;
}

/*
 * Write a file name to the session file.
 * Takes care of the "slash" option in 'sessionoptions' and escapes special
 * characters.
 * Returns FAIL if writing fails.
 */
    static int
ses_put_fname(fd, name, flagp)
    FILE	*fd;
    char_u	*name;
    unsigned	*flagp;
{
    char_u	*sname;
    int		retval = OK;
    int		c;

    sname = home_replace_save(NULL, name);
    if (sname != NULL)
	name = sname;
    while (*name != NUL)
    {
#ifdef FEAT_MBYTE
	{
	    int l;

	    if (has_mbyte && (l = (*mb_ptr2len_check)(name)) > 1)
	    {
		/* copy a multibyte char */
		while (--l >= 0)
		{
		    if (putc(*name, fd) != *name)
			retval = FAIL;
		    ++name;
		}
		continue;
	    }
	}
#endif
	c = *name++;
	if (c == '\\' && (*flagp & SSOP_SLASH))
	    /* change a backslash to a forward slash */
	    c = '/';
	else if ((vim_strchr(escape_chars, c) != NULL
#ifdef BACKSLASH_IN_FILENAME
		    && c != '\\'
#endif
		 ) || c == '#' || c == '%')
	{
	    /* escape a special character with a backslash */
	    if (putc('\\', fd) != '\\')
		retval = FAIL;
	}
	if (putc(c, fd) != c)
	    retval = FAIL;
    }
    vim_free(sname);
    return retval;
}

/*
 * ":loadview [nr]"
 */
    static void
ex_loadview(eap)
    exarg_T	*eap;
{
    char_u	*fname;

    fname = get_view_file(*eap->arg);
    if (fname != NULL)
    {
	do_source(fname, FALSE, FALSE);
	vim_free(fname);
    }
}

/*
 * Get the name of the view file for the current buffer.
 */
    static char_u *
get_view_file(c)
    int		c;
{
    int		len = 0;
    char_u	*p, *s;
    char_u	*retval;
    char_u	*sname;

    if (curbuf->b_ffname == NULL)
    {
	EMSG(_(e_noname));
	return NULL;
    }
    sname = home_replace_save(NULL, curbuf->b_ffname);
    if (sname == NULL)
	return NULL;

    /*
     * We want a file name without separators, because we're not going to make
     * a directory.
     * "normal" path separator	-> "=+"
     * "="			-> "=="
     * ":" path separator	-> "=-"
     */
    for (p = sname; *p; ++p)
	if (*p == '=' || vim_ispathsep(*p))
	    ++len;
    retval = alloc((unsigned)(STRLEN(sname) + len + STRLEN(p_vdir) + 9));
    if (retval != NULL)
    {
	STRCPY(retval, p_vdir);
	add_pathsep(retval);
	s = retval + STRLEN(retval);
	for (p = sname; *p; ++p)
	{
	    if (*p == '=')
	    {
		*s++ = '=';
		*s++ = '=';
	    }
	    else if (vim_ispathsep(*p))
	    {
		*s++ = '=';
#ifdef MACOS_CLASSIC /* TODO: Is it also needed for MACOS_X? (Dany) */
		*s++ = '+';
#else
# if defined(BACKSLASH_IN_FILENAME) || defined(AMIGA) || defined(RISCOS) \
	|| defined(VMS)
		if (*p == ':')
		    *s++ = '-';
		else
# endif
		    *s++ = '+';
#endif
	    }
	    else
		*s++ = *p;
	}
	*s++ = '=';
	*s++ = c;
	STRCPY(s, ".vim");
    }

    vim_free(sname);
    return retval;
}

#endif /* FEAT_SESSION */

/*
 * Write end-of-line character(s) for ":mkexrc", ":mkvimrc" and ":mksession".
 * Return FAIL for a write error.
 */
    int
put_eol(fd)
    FILE	*fd;
{
    if (
#ifdef USE_CRNL
	    (
# ifdef MKSESSION_NL
	     !mksession_nl &&
# endif
	     (putc('\r', fd) < 0)) ||
#endif
	    (putc('\n', fd) < 0))
	return FAIL;
    return OK;
}

/*
 * Write a line to "fd".
 * Return FAIL for a write error.
 */
    int
put_line(fd, s)
    FILE	*fd;
    char	*s;
{
    if (fputs(s, fd) < 0 || put_eol(fd) == FAIL)
	return FAIL;
    return OK;
}

#ifdef FEAT_VIMINFO
/*
 * ":rviminfo" and ":wviminfo".
 */
    static void
ex_viminfo(eap)
    exarg_T	*eap;
{
    char_u	*save_viminfo;

    save_viminfo = p_viminfo;
    if (*p_viminfo == NUL)
	p_viminfo = (char_u *)"'100";
    if (eap->cmdidx == CMD_rviminfo)
    {
	if (read_viminfo(eap->arg, TRUE, TRUE, eap->forceit) == FAIL)
	    EMSG(_("E195: Cannot open viminfo file for reading"));
    }
    else
	write_viminfo(eap->arg, eap->forceit);
    p_viminfo = save_viminfo;
}
#endif

#if defined(FEAT_GUI_DIALOG) || defined(FEAT_CON_DIALOG) || defined(PROTO)
    void
dialog_msg(buff, format, fname)
    char_u	*buff;
    char	*format;
    char_u	*fname;
{
    int		len;

    if (fname == NULL)
	fname = (char_u *)_("Untitled");
    len = (int)STRLEN(format) + (int)STRLEN(fname);
    if (len >= IOSIZE)
	sprintf((char *)buff, format, (int)(IOSIZE - STRLEN(format)), fname);
    else
	sprintf((char *)buff, format, (int)STRLEN(fname), fname);
}
#endif

/*
 * ":behave {mswin,xterm}"
 */
    static void
ex_behave(eap)
    exarg_T	*eap;
{
    if (STRCMP(eap->arg, "mswin") == 0)
    {
	set_option_value((char_u *)"selection", 0L, (char_u *)"exclusive", 0);
	set_option_value((char_u *)"selectmode", 0L, (char_u *)"mouse,key", 0);
	set_option_value((char_u *)"mousemodel", 0L, (char_u *)"popup", 0);
	set_option_value((char_u *)"keymodel", 0L,
					     (char_u *)"startsel,stopsel", 0);
    }
    else if (STRCMP(eap->arg, "xterm") == 0)
    {
	set_option_value((char_u *)"selection", 0L, (char_u *)"inclusive", 0);
	set_option_value((char_u *)"selectmode", 0L, (char_u *)"", 0);
	set_option_value((char_u *)"mousemodel", 0L, (char_u *)"extend", 0);
	set_option_value((char_u *)"keymodel", 0L, (char_u *)"", 0);
    }
    else
	EMSG2(_(e_invarg2), eap->arg);
}

#ifdef FEAT_AUTOCMD
static int filetype_detect = FALSE;
static int filetype_plugin = FALSE;
static int filetype_indent = FALSE;

/*
 * ":filetype [plugin] [indent] {on,off,detect}"
 * on: Load the filetype.vim file to install autocommands for file types.
 * off: Load the ftoff.vim file to remove all autocommands for file types.
 * plugin on: load filetype.vim and ftplugin.vim
 * plugin off: load ftplugof.vim
 * indent on: load filetype.vim and indent.vim
 * indent off: load indoff.vim
 */
    static void
ex_filetype(eap)
    exarg_T	*eap;
{
    char_u	*arg = eap->arg;
    int		plugin = FALSE;
    int		indent = FALSE;

    if (*eap->arg == NUL)
    {
	/* Print current status. */
	smsg((char_u *)"filetype detection:%s  plugin:%s  indent:%s",
		filetype_detect ? "ON" : "OFF",
		filetype_plugin ? (filetype_detect ? "ON" : "(on)") : "OFF",
		filetype_indent ? (filetype_detect ? "ON" : "(on)") : "OFF");
	return;
    }

    /* Accept "plugin" and "indent" in any order. */
    for (;;)
    {
	if (STRNCMP(arg, "plugin", 6) == 0)
	{
	    plugin = TRUE;
	    arg = skipwhite(arg + 6);
	    continue;
	}
	if (STRNCMP(arg, "indent", 6) == 0)
	{
	    indent = TRUE;
	    arg = skipwhite(arg + 6);
	    continue;
	}
	break;
    }
    if (STRCMP(arg, "on") == 0 || STRCMP(arg, "detect") == 0)
    {
	if (*arg == 'o' || !filetype_detect)
	{
	    cmd_runtime((char_u *)FILETYPE_FILE, TRUE);
	    filetype_detect = TRUE;
	    if (plugin)
	    {
		cmd_runtime((char_u *)FTPLUGIN_FILE, TRUE);
		filetype_plugin = TRUE;
	    }
	    if (indent)
	    {
		cmd_runtime((char_u *)INDENT_FILE, TRUE);
		filetype_indent = TRUE;
	    }
	}
	if (*arg == 'd')
	{
	    (void)do_doautocmd((char_u *)"filetypedetect BufRead", TRUE);
	    do_modelines();
	}
    }
    else if (STRCMP(arg, "off") == 0)
    {
	if (plugin || indent)
	{
	    if (plugin)
	    {
		cmd_runtime((char_u *)FTPLUGOF_FILE, TRUE);
		filetype_plugin = FALSE;
	    }
	    if (indent)
	    {
		cmd_runtime((char_u *)INDOFF_FILE, TRUE);
		filetype_indent = FALSE;
	    }
	}
	else
	{
	    cmd_runtime((char_u *)FTOFF_FILE, TRUE);
	    filetype_detect = FALSE;
	}
    }
    else
	EMSG2(_(e_invarg2), arg);
}

/*
 * ":setfiletype {name}"
 */
    static void
ex_setfiletype(eap)
    exarg_T	*eap;
{
    if (!did_filetype)
	set_option_value((char_u *)"filetype", 0L, eap->arg, OPT_LOCAL);
}
#endif

/*ARGSUSED*/
    static void
ex_digraphs(eap)
    exarg_T	*eap;
{
#ifdef FEAT_DIGRAPHS
    if (*eap->arg != NUL)
	putdigraph(eap->arg);
    else
	listdigraphs();
#else
    EMSG(_("E196: No digraphs in this version"));
#endif
}

    static void
ex_set(eap)
    exarg_T	*eap;
{
    int		flags = 0;

    if (eap->cmdidx == CMD_setlocal)
	flags = OPT_LOCAL;
    else if (eap->cmdidx == CMD_setglobal)
	flags = OPT_GLOBAL;
#if defined(FEAT_EVAL) && defined(FEAT_AUTOCMD) && defined(FEAT_BROWSE)
    if (cmdmod.browse && flags == 0)
	ex_options(eap);
    else
#endif
	(void)do_set(eap->arg, flags);
}

#ifdef FEAT_SEARCH_EXTRA
/*
 * ":nohlsearch"
 */
/*ARGSUSED*/
    static void
ex_nohlsearch(eap)
    exarg_T	*eap;
{
    no_hlsearch = TRUE;
    redraw_all_later(NOT_VALID);
}

/*
 * ":match {group} {pattern}"
 * Sets nextcmd to the start of the next command, if any.  Also called when
 * skipping commands to find the next command.
 */
    static void
ex_match(eap)
    exarg_T	*eap;
{
    char_u	*p;
    char_u	*end;
    int		c;

    /* First clear any old pattern. */
    if (!eap->skip)
    {
	vim_free(curwin->w_match.regprog);
	curwin->w_match.regprog = NULL;
	redraw_later(NOT_VALID);	/* always need a redraw */
    }

    if (ends_excmd(*eap->arg))
	end = eap->arg;
    else if ((STRNICMP(eap->arg, "none", 4) == 0
		&& (vim_iswhite(eap->arg[4]) || ends_excmd(eap->arg[4]))))
	end = eap->arg + 4;
    else
    {
	p = skiptowhite(eap->arg);
	if (!eap->skip)
	{
	    curwin->w_match_id = syn_namen2id(eap->arg, (int)(p - eap->arg));
	    if (curwin->w_match_id == 0)
	    {
		EMSG2(_(e_nogroup), eap->arg);
		return;
	    }
	}
	p = skipwhite(p);
	if (*p == NUL)
	{
	    /* There must be two arguments. */
	    EMSG2(_(e_invarg2), eap->arg);
	    return;
	}
	end = skip_regexp(p + 1, *p, TRUE, NULL);
	if (!eap->skip)
	{
	    if (*end != NUL && !ends_excmd(*skipwhite(end + 1)))
	    {
		eap->errmsg = e_trailing;
		return;
	    }

	    c = *end;
	    *end = NUL;
	    curwin->w_match.regprog = vim_regcomp(p + 1, RE_MAGIC);
	    *end = c;
	    if (curwin->w_match.regprog == NULL)
	    {
		EMSG2(_(e_invarg2), p);
		return;
	    }
	}
    }
    eap->nextcmd = find_nextcmd(end);
}
#endif

#ifdef FEAT_CRYPT
/*
 * ":X": Get crypt key
 */
/*ARGSUSED*/
    static void
ex_X(eap)
    exarg_T	*eap;
{
    (void)get_crypt_key(TRUE, TRUE);
}
#endif

#ifdef FEAT_FOLDING
    static void
ex_fold(eap)
    exarg_T	*eap;
{
    if (foldManualAllowed(TRUE))
	foldCreate(eap->line1, eap->line2);
}

    static void
ex_foldopen(eap)
    exarg_T	*eap;
{
    opFoldRange(eap->line1, eap->line2, eap->cmdidx == CMD_foldopen,
							 eap->forceit, FALSE);
}

    static void
ex_folddo(eap)
    exarg_T	*eap;
{
    linenr_T	lnum;

    /* First set the marks for all lines closed/open. */
    for (lnum = eap->line1; lnum <= eap->line2; ++lnum)
	if (hasFolding(lnum, NULL, NULL) == (eap->cmdidx == CMD_folddoclosed))
	    ml_setmarked(lnum);

    /* Execute the command on the marked lines. */
    global_exe(eap->arg);
    ml_clearmarked();	   /* clear rest of the marks */
}
#endif
