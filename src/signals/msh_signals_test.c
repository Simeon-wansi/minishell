#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <sys/wait.h>
#include <errno.h>

// Forward declaration of the types and functions
typedef enum {
    NODE_CMD,
    NODE_PIPE,
    NODE_REDIR,
    // Add other node types as needed
} t_node_type;

typedef struct s_io {
    char *heredoc_delim;  // Delimiter for heredoc
    // Add other IO redirection fields as needed
} t_io;

typedef struct s_cmd_node {
    char **args;          // Command arguments
    t_io *io;             // IO redirections
    // Add other command node fields as needed
} t_cmd_node;

typedef struct s_ast {
    t_node_type type;     // Type of the node
    union {
        t_cmd_node cmd_node;  // Command node data
        // Add other node data types as needed
    } data;
    // Add other AST node fields as needed
} t_ast;

// Function prototypes from the provided code
void signal_handler_interactive(int signum);
void signal_handler_heredoc2(int signum, siginfo_t *info, void *context);
void signal_handler_child(int signum);
void setup_signals_interactive(void);
void setup_signals_heredoc(pid_t child_pid);
void setup_signals_child(void);
void reset_signals(void);
void set_signal_handler(t_ast *node);
void set_heredoc_pid(pid_t pid);

// Signal handler for the interactive shell prompt
void signal_handler_interactive(int signum)
{
    if (signum == SIGINT)
    {
        write(STDERR_FILENO, "\n", 1);
        rl_replace_line("", 0);
        rl_on_new_line();
        rl_redisplay();
    }
    // SIGQUIT is ignored in this mode (handled by setup_signals_interactive)
}

// Signal handler for heredoc operations
void signal_handler_heredoc2(int signum, siginfo_t *info, void *context)
{
    (void)context; // Unused parameter
    
    if (signum == SIGINT)
    {
        write(STDOUT_FILENO, "\n", 1);
        // info->si_value.sival_int contains child PID passed in setup_signals_heredoc
        if (info->si_value.sival_int > 0)
            kill(info->si_value.sival_int, SIGINT);
    }
}

// Signal handler for child process execution
void signal_handler_child(int signum)
{
    if (signum == SIGINT)
    {
        write(STDERR_FILENO, "\n", 1);
    }
    else if (signum == SIGQUIT)
    {
        write(STDERR_FILENO, "Quit: 3\n", 8);
    }
}

// Set up signal handlers for interactive shell mode
void setup_signals_interactive(void)
{
    struct sigaction sa = {0};
    
    sa.sa_handler = signal_handler_interactive;
    sa.sa_flags = SA_RESTART;
    sigemptyset(&sa.sa_mask);
    
    // Add all signals to the mask to prevent nested signal handling
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);
    sigaddset(&sa.sa_mask, SIGTSTP);
    
    sigaction(SIGINT, &sa, NULL);
    
    // Ignore SIGQUIT and SIGTSTP in interactive mode
    sa.sa_handler = SIG_IGN;
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
}

// Set up signal handlers for heredoc operations
void setup_signals_heredoc(pid_t child_pid)
{
    struct sigaction sa = {0};
    union sigval value;
    
    // Store the child PID in the sigaction context
    value.sival_int = child_pid;
    
    // Use sa_sigaction instead of sa_handler to get more context
    sa.sa_sigaction = signal_handler_heredoc2;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;  // SA_SIGINFO is crucial here
    sigemptyset(&sa.sa_mask);
    
    sigaddset(&sa.sa_mask, SIGINT);
    sigaddset(&sa.sa_mask, SIGQUIT);
    
    // Set up the signal handler with the child PID context
    sigaction(SIGINT, &sa, NULL);
    
    // Also register the PID with the existing handler by sending a signal with data
    sigqueue(getpid(), SIGUSR1, value);
    
    // Ignore SIGQUIT in heredoc mode
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = SA_RESTART;  // Reset flags
    sigaction(SIGQUIT, &sa, NULL);
}

// Set up signal handlers for child process execution
void setup_signals_child(void)
{
    struct sigaction sa = {0};
    
    sa.sa_handler = signal_handler_child;
    sigemptyset(&sa.sa_mask);
    
    // No SA_RESTART - we want interrupted system calls to fail with EINTR
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    
    // Default action for SIGTSTP
    sa.sa_handler = SIG_DFL;
    sigaction(SIGTSTP, &sa, NULL);
}

// Reset signals to default behavior (useful after fork)
void reset_signals(void)
{
    struct sigaction sa = {0};
    
    sa.sa_handler = SIG_DFL;
    sigemptyset(&sa.sa_mask);
    
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGQUIT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
}

// Global variable to store heredoc child PID
static pid_t stored_heredoc_pid = 0;

// Main function to set up signal handlers based on the current operation
void set_signal_handler(t_ast *node)
{
    if (node && node->type == NODE_CMD &&
        node->data.cmd_node.io &&
        node->data.cmd_node.io->heredoc_delim != NULL)
    {
        // Setup for heredoc operation with the stored PID
        setup_signals_heredoc(stored_heredoc_pid);
    }
    else if (node)
    {
        // When executing commands
        setup_signals_child();
    }
    else
    {
        // Back to interactive mode
        setup_signals_interactive();
    }
}

// Function to store child PID for heredoc operations
void set_heredoc_pid(pid_t pid)
{
    stored_heredoc_pid = pid;
    
    struct sigaction sa = {0};
    union sigval value;
    
    value.sival_int = pid;
    
    // Create a handler for SIGUSR1 that just stores the PID
    sa.sa_sigaction = signal_handler_heredoc2;
    sa.sa_flags = SA_RESTART | SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    
    // Send a signal to self with the PID as data
    sigqueue(getpid(), SIGUSR1, value);
}

// Helper function to create a command AST node with heredoc
t_ast *create_heredoc_cmd(const char *cmd, const char *heredoc_delim) {
    t_ast *node = (t_ast *)malloc(sizeof(t_ast));
    if (!node)
        return NULL;
    
    node->type = NODE_CMD;
    
    // Allocate and set command arguments
    node->data.cmd_node.args = (char **)malloc(2 * sizeof(char *));
    if (!node->data.cmd_node.args) {
        free(node);
        return NULL;
    }
    node->data.cmd_node.args[0] = strdup(cmd);
    node->data.cmd_node.args[1] = NULL;
    
    // Allocate and set IO structure
    node->data.cmd_node.io = (t_io *)malloc(sizeof(t_io));
    if (!node->data.cmd_node.io) {
        free(node->data.cmd_node.args[0]);
        free(node->data.cmd_node.args);
        free(node);
        return NULL;
    }
    
    // Set heredoc delimiter
    if (heredoc_delim)
        node->data.cmd_node.io->heredoc_delim = strdup(heredoc_delim);
    else
        node->data.cmd_node.io->heredoc_delim = NULL;
    
    return node;
}

// Helper function to create a regular command AST node
t_ast *create_cmd(const char *cmd) {
    t_ast *node = (t_ast *)malloc(sizeof(t_ast));
    if (!node)
        return NULL;
    
    node->type = NODE_CMD;
    
    // Allocate and set command arguments
    node->data.cmd_node.args = (char **)malloc(2 * sizeof(char *));
    if (!node->data.cmd_node.args) {
        free(node);
        return NULL;
    }
    node->data.cmd_node.args[0] = strdup(cmd);
    node->data.cmd_node.args[1] = NULL;
    
    // Allocate and set IO structure with no heredoc
    node->data.cmd_node.io = (t_io *)malloc(sizeof(t_io));
    if (!node->data.cmd_node.io) {
        free(node->data.cmd_node.args[0]);
        free(node->data.cmd_node.args);
        free(node);
        return NULL;
    }
    node->data.cmd_node.io->heredoc_delim = NULL;
    
    return node;
}

// Helper function to free an AST node
void free_ast_node(t_ast *node) {
    if (!node)
        return;
    
    if (node->type == NODE_CMD) {
        if (node->data.cmd_node.args) {
            for (int i = 0; node->data.cmd_node.args[i]; i++)
                free(node->data.cmd_node.args[i]);
            free(node->data.cmd_node.args);
        }
        
        if (node->data.cmd_node.io) {
            if (node->data.cmd_node.io->heredoc_delim)
                free(node->data.cmd_node.io->heredoc_delim);
            free(node->data.cmd_node.io);
        }
    }
    
    free(node);
}

// Test interactive mode with signal handling
void test_interactive_mode() {
    printf("\n=== Testing Interactive Mode ===\n");
    printf("Press Ctrl+C to test SIGINT handling\n");
    printf("Press Ctrl+\\ to test SIGQUIT handling (should be ignored)\n");
    printf("Press Ctrl+Z to test SIGTSTP handling (should be ignored)\n");
    printf("Type 'exit' or press Enter to continue to the next test\n\n");
    
    // Set up signal handlers for interactive mode
    setup_signals_interactive();
    
    char *line = NULL;
    while (1) {
        line = readline("minishell> ");
        if (!line || strcmp(line, "exit") == 0 || strlen(line) == 0) {
            if (line)
                free(line);
            break;
        }
        
        printf("You entered: %s\n", line);
        add_history(line);
        free(line);
    }
}

// Test heredoc mode with signal handling
void test_heredoc_mode() {
    printf("\n=== Testing Heredoc Mode ===\n");
    printf("Press Ctrl+C to test SIGINT handling in heredoc\n");
    printf("Press Ctrl+\\ to test SIGQUIT handling (should be ignored)\n");
    printf("Type 'EOF' or press Enter to continue to the next test\n\n");
    
    // Create a command node with heredoc
    t_ast *node = create_heredoc_cmd("cat", "EOF");
    
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        reset_signals(); // Reset signals in child
        sleep(30);       // Child will run for a while
        exit(0);
    } else if (pid > 0) {
        // Parent process
        // Register the child PID for heredoc operations
        set_heredoc_pid(pid);
        set_signal_handler(node);
        
        printf("Simulating heredoc input (child PID: %d)\n", pid);
        printf("Enter text, end with 'EOF' on a line by itself:\n");
        
        char *line = NULL;
        while (1) {
            line = readline("heredoc> ");
            if (!line || strcmp(line, "EOF") == 0 || strlen(line) == 0) {
                if (line)
                    free(line);
                break;
            }
            
            printf("Line: %s\n", line);
            free(line);
        }
        
        // Terminate and clean up child
        kill(pid, SIGTERM);
        waitpid(pid, NULL, 0);
    } else {
        perror("fork");
    }
    
    free_ast_node(node);
}

// Test child execution mode with signal handling
void test_child_mode() {
    printf("\n=== Testing Child Execution Mode ===\n");
    printf("Press Ctrl+C to test SIGINT handling during command execution\n");
    printf("Press Ctrl+\\ to test SIGQUIT handling\n");
    printf("Press Ctrl+Z to test SIGTSTP handling (should suspend)\n");
    printf("Type 'exit' or press Enter to end the test\n\n");
    
    // Create a regular command node
    t_ast *node = create_cmd("sleep");
    
    char *line = NULL;
    while (1) {
        line = readline("cmd> ");
        if (!line || strcmp(line, "exit") == 0 || strlen(line) == 0) {
            if (line)
                free(line);
            break;
        }
        
        printf("Executing: %s (simulated)\n", line);
        
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            reset_signals();
            setup_signals_child();
            printf("