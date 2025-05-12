/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   pwd.c                                              :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: sngantch <sngantch@student.42abudhabi.a    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/05/06 21:34:14 by sngantch          #+#    #+#             */
/*   Updated: 2025/05/06 22:32:09 by sngantch         ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "builtins.h"

void builtin_pwd(int *exit_status)
{
    char *cwd;

    cwd = getcwd(NULL, 0);
        
    if (cwd != NULL)
    {
        ft_printf("%s\n", cwd);
        free(cwd);
        *exit_status = 0;
    }
    else
    {
        perror("pwd");
        *exit_status = 1;
    }
}