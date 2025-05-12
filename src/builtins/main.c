/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.c                                             :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sngantch <sngantch@student.42abudhabi.a    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 21:36:57 by sngantch          #+#    #+#             */
/*   Updated: 2025/05/08 20:43:59 by sngantch         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "builtins.h" 
#include "../signals/msh_signals.h"

#include <stdarg.h>

int is_builtin(char *cmd)
{
	return (cmd && (!strcmp(cmd, "pwd") || !strcmp(cmd, "cd")));
}

int exec_builtin(char **argv, int *exit_status)
{
	if (!strcmp(argv[0], "pwd"))
		return (builtin_pwd(exit_status), *exit_status);
	else if (!strcmp(argv[0], "cd"))
		return builtin_cd(argv, exit_status);
	return 1;
}

void run_external_command(char **argv, int *exit_status)
{
	pid_t pid = fork();

	if (pid == 0)
	{
		signal(SIGINT, SIG_DFL);
		signal(SIGQUIT, SIG_DFL);
		execvp(argv[0], argv);
		perror("execvp");
		exit(EXIT_FAILURE);
	}
	else if (pid > 0)
	{
		int status;
		waitpid(pid, &status, 0);

		if (WIFSIGNALED(status))
		{
			int sig = WTERMSIG(status);
			if (sig == SIGQUIT)
				dprintf(STDERR_FILENO, "Quit: %d\n", sig);
			else if (sig == SIGINT)
				dprintf(STDERR_FILENO, "\n");
			*exit_status = 128 + sig;
		}
		else if (WIFEXITED(status))
		{
			*exit_status = WEXITSTATUS(status);
		}
	}
	else
	{
		perror("fork");
		*exit_status = 1;
	}
}

int main(void)
{
	char *input;
	int exit_status = 0;

	while (1)
	{
		setup_signals();
		input = readline("minishell> ");
		if (!input)
		{
			printf("exit\n");
			break;
		}

		if (strcmp(input, "exit") == 0)
		{
			free(input);
			break;
		}

		// Tokenize input
		char *copy = strdup(input);
		char *argv[256];
		char *token = strtok(copy, " ");
		int i = 0;
		while (token && i < 255)
			argv[i++] = token, token = strtok(NULL, " ");
		argv[i] = NULL;

		if (!argv[0])
		{
			free(input);
			free(copy);
			continue;
		}

		if (is_builtin(argv[0]))
			exec_builtin(argv, &exit_status);
		else
			run_external_command(argv, &exit_status);

		free(input);
		free(copy);
	}
	return exit_status;
}
