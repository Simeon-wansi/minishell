/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   cd.c                                               :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sngantch <sngantch@student.42abudhabi.a    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 21:33:48 by sngantch          #+#    #+#             */
/*   Updated: 2025/05/08 20:47:09 by sngantch         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "builtins.h"


#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

// Update PWD and OLDPWD using setenv (or custom env functions later)
static void update_env_pwd()
{
	char cwd[4096];
	char *oldpwd = getenv("PWD");

	if (oldpwd)
		setenv("OLDPWD", oldpwd, 1);

	if (getcwd(cwd, sizeof(cwd)) != NULL)
		setenv("PWD", cwd, 1);
}

int builtin_cd(char **argv, int *exit_status)
{
	const char *target;
	char *home = getenv("HOME");
	char *oldpwd = getenv("OLDPWD");

	if (!argv[1])
		target = home;
	else if (strcmp(argv[1], "-") == 0)
	{
		if (!oldpwd)
		{
			write(stderr, "cd: OLDPWD not set\n", 19);
			*exit_status = 1;
			return 1;
		}
		target = oldpwd;
		ft_printf("%s\n", target); // cd - should print path
	}
	else
		target = argv[1];

	if (!target)
	{
		write(stderr, "cd: HOME not set\n", 16);
		*exit_status = 1;
		return 1;
	}

	if (chdir(target) != 0)
	{
		ft_printf("cd: %s: %s", target, strerror(errno));
		write(stderr, "\n", 1);
		*exit_status = 1;
		return 1;
	}

	update_env_pwd();
	*exit_status = 0;
	return 0;
}
