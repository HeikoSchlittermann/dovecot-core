/* Copyright (c) 2009-2010 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "array.h"
#include "master-service.h"
#include "doveadm-mail.h"
#include "doveadm.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

bool doveadm_verbose = FALSE, doveadm_debug = FALSE;

static ARRAY_DEFINE(doveadm_cmds, struct doveadm_cmd);

void doveadm_register_cmd(const struct doveadm_cmd *cmd)
{
	array_append(&doveadm_cmds, cmd, 1);
}

void usage(void)
{
	const struct doveadm_cmd *cmd;

	fprintf(stderr, "usage: doveadm\n");

	array_foreach(&doveadm_cmds, cmd) {
		fprintf(stderr, USAGE_CMDNAME_FMT" %s\n",
			cmd->name, cmd->short_usage);
	}
	doveadm_mail_usage();
	exit(1);
}

void help(const struct doveadm_cmd *cmd)
{
	fprintf(stderr, "doveadm %s %s\n", cmd->name, cmd->short_usage);
	if (cmd->long_usage != NULL)
		fprintf(stderr, "%s", cmd->long_usage);
	exit(0);
}

const char *unixdate2str(time_t timestamp)
{
	static char buf[64];
	struct tm *tm;

	tm = localtime(&timestamp);
	strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
	return buf;
}

static void cmd_help(int argc ATTR_UNUSED, char *argv[])
{
	const struct doveadm_cmd *cmd;

	if (argv[1] == NULL)
		usage();
	array_foreach(&doveadm_cmds, cmd) {
		if (strcmp(cmd->name, argv[1]) == 0)
			help(cmd);
	}
	doveadm_mail_help_name(argv[1]);
	usage();
}

static struct doveadm_cmd doveadm_cmd_help = {
	cmd_help, "help", "<cmd>", NULL
};

static bool doveadm_try_run(const char *cmd_name, int argc, char *argv[])
{
	const struct doveadm_cmd *cmd;

	array_foreach(&doveadm_cmds, cmd) {
		if (strcmp(cmd_name, cmd->name) == 0) {
			cmd->cmd(argc, argv);
			return TRUE;
		}
	}
	return FALSE;
}

int main(int argc, char *argv[])
{
	const char *cmd_name;
	int c;

	/* "+" is GNU extension to stop at the first non-option.
	   others just accept -+ option. */
	master_service = master_service_init("doveadm",
					     MASTER_SERVICE_FLAG_STANDALONE,
					     &argc, &argv, "+Dv");
	i_array_init(&doveadm_cmds, 32);
	doveadm_mail_init();
	doveadm_register_cmd(&doveadm_cmd_help);
	doveadm_register_cmd(&doveadm_cmd_auth);
	doveadm_register_cmd(&doveadm_cmd_user);
	doveadm_register_cmd(&doveadm_cmd_dump);
	doveadm_register_cmd(&doveadm_cmd_pw);
	doveadm_register_cmd(&doveadm_cmd_who);
	doveadm_register_cmd(&doveadm_cmd_penalty);

	while ((c = master_getopt(master_service)) > 0) {
		switch (c) {
		case 'D':
			doveadm_debug = TRUE;
			doveadm_verbose = TRUE;
			break;
		case 'v':
			doveadm_verbose = TRUE;
			break;
		default:
			return FATAL_DEFAULT;
		}
	}
	if (optind == argc)
		usage();

	cmd_name = argv[optind];
	argc -= optind;
	argv += optind;
	optind = 1;

	master_service_init_finish(master_service);
	if (!doveadm_try_run(cmd_name, argc, argv) &&
	    !doveadm_mail_try_run(cmd_name, argc, argv))
		usage();

	master_service_deinit(&master_service);
	doveadm_mail_deinit();
	array_free(&doveadm_cmds);
	return 0;
}
