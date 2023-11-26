/* Parse tree using ASCII graphics
        -original NCurses code from "Game Programming in C with the Ncurses Library"
         https://www.viget.com/articles/game-programming-in-c-with-the-ncurses-library/
         and from "NCURSES Programming HOWTO"
         http://tldp.org/HOWTO/NCURSES-Programming-HOWTO/
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#ifndef NOGRAPHICS
#include <unistd.h>
#include <ncurses.h>
#endif

#define MAXVARNAME 10
#define MAXVAR 1000
#define SCREENSIZE 200

// Syntax flags for the interpreter
#define INT 1
#define SET 2
#define BEGIN 3
#define END 4
#define ADD 5
#define SUB 6
#define MULT 7
#define DIV 8
#define PRINT 9
#define GOTO 10
#define IF 11

// Command structure
typedef struct
{
   // line number of the command
   int line_number;

   // type of command, see syntax flags above
   int command_type;

   // arguments are stored as strings for the command only
   // Values are stored as integers in the runtime structure once the program is executed
   char *args[3];
} Command;

// Runtime structure
typedef struct
{
   // Realistically I would have used a HashMap for this by using the line number as the key and the command as the value
   Command **commands;
   int commandsLen;

   // Track the length of the int names and int values arrays
   int intNamesLen;
   int intValuesLen;

   // Used to store the names of the int variables
   char *intNames[MAXVAR];

   // Used to store the values of the int variables
   int intValues[MAXVAR];

   // If a variable is set, the value is 1, otherwise it is 0
   int intValuesSet[MAXVAR];

   // Program counter and begin and end line numbers
   int pc;
   int begin_line;
   int end_line;

   // Flags to check if begin and end commands are present
   int begin_flag;
   int end_flag;

} Runtime;

// Runtime functions
Runtime *build_runtime_from_file(const char *filename);
void execute_runtime(Runtime *runtime);
void free_runtime(Runtime *runtime);

// Parse functions
int parse_line(Runtime *runtime, int n, char *line);
int parse_arg(Runtime *runtime, int n, char *token, int i);

// Helper functions
void print_runtime(Runtime *runtime);
int is_defined(Runtime *runtime, char *arg);
int is_set(Runtime *runtime, char *arg);
int evaluate(Runtime *runtime, int n, char *op, int val1, int val2);
int check_arguments(int command_type, int argc, int line_number);
int determine_command_type(char *token);
int get_command_by_line_number(Runtime *runtime, int line_number);
const char *command_type_to_string(int command_type);
int is_integer(char *token);

#ifndef NOGRAPHICS
// curses output
// row indicates in which row the output will start - larger numbers
//      move down
// col indicates in which column the output will start - larger numbers
//      move to the right
// when row,col == 0,0 it is the upper left hand corner of the window
void print(int row, int col, char *str)
{
   mvprintw(row, col, str);
}
#endif

int main(int argc, char *argv[])
{
   int c;

   // check for correct number of arguments
   if (argc != 2)
   {
      printf("Usage: %s <filename>\n", argv[0]);
      return -1;
   }

   const char *filename = argv[1];

#ifndef NOGRAPHICS
   // initialize ncurses
   initscr();
   noecho();
   cbreak();
   timeout(0);
   curs_set(FALSE);
#endif

   /* read and interpret the file starting here */
   Runtime *runtime = build_runtime_from_file(filename);

   if (runtime == NULL)
   {
      printf("Error: Could not build runtime\n");
      return -1;
   }

   // Print the runtime structure
   // print_runtime(runtime);

   // Execute the program
   execute_runtime(runtime);

#ifndef NOGRAPHICS
   int i = 0;

   /* loop until the 'q' key is pressed */
   while (1)
   {
      c = getch();
      if (c == 'q')
         break;
   }

   // shut down ncurses
   endwin();
#endif

   // Free the runtime structure
   free_runtime(runtime);
   return 0;
}

# pragma region Runtime Functions

// read the file and build the runtime structure
Runtime *build_runtime_from_file(const char *filename)
{
   FILE *fp;
   char *line = NULL;
   size_t len = 0;
   ssize_t read;

   // open the file, return -1 if the file cannot be opened
   fp = fopen(filename, "r");
   if (fp == NULL)
   {
      printf("Error opening file %s\n", filename);
      return NULL;
   }

   // Number of lines read
   int n = 0;

   // Count the number of valid lines in the file to initialize the command array
   while ((read = getline(&line, &len, fp)) != -1)
   {
      // Check if the line is not an empty newline
      if(line[0] != '\n')
         n++;
   }

   // Reset the file pointer to the beginning of the file
   fseek(fp, 0, SEEK_SET);

   // Initialize program runtime
   Runtime *runtime = malloc(sizeof(Runtime));

   // Initialize the array of commands
   runtime->commands = malloc(sizeof(Command *) * n);

   // Initialize the individual commands
   for (int i = 0; i < n; i++)
   {
      runtime->commands[i] = malloc(sizeof(Command));
   }

   // Set current lengths for int names and int values
   runtime->intNamesLen = 0;
   runtime->intValuesLen = 0;
   runtime->commandsLen = n;

   for(int i = 0; i < MAXVAR; i++)
   {
      runtime->intValuesSet[i] = 0;
   }

   // Check if malloc failed
   if (runtime->commands == NULL)
   {
      printf("Error: Could not allocate memory for commands\n");

      // Close the file
      fclose(fp);
      return NULL;
   }

   // reset n to 0 so that it can be used as an index
   n = 0;

   // Read the file line by line
   while ((read = getline(&line, &len, fp)) != -1)
   {
      if (line[0] == '\n')
         continue;

      // Parse the line
      if (parse_line(runtime, n, line) == -1)
      {
         // Close the file
         fclose(fp);
         return NULL;
      }
      n++;
   }

   free(line);
   fclose(fp);

   // Check if the begin command is present
   if (runtime->begin_flag == 0)
   {
      printf("Error: No begin command\n");
      free_runtime(runtime);
      return NULL;
   }

   // Check if the end command is present
   if (runtime->end_flag == 0)
   {
      printf("Error: No end command\n");
      free_runtime(runtime);
      return NULL;
   }

   return runtime;
}

// Execute runtime and step through commands
void execute_runtime(Runtime *runtime)
{
   if (runtime == NULL)
   {
      return;
   }

   // Set the program counter to the begin line
   runtime->pc = runtime->begin_line;

   int expression_flag;

   //Loop through commands while runtime->pc is less than the end line
   while(runtime->pc < runtime->end_line)
   {
      // Get command at program counter
      int command_id = get_command_by_line_number(runtime, runtime->pc);
      if (command_id == -1)
      {
         printf("Error: Command at line %d not found\n", runtime->pc);
         return;
      }
      Command *cur_command = runtime->commands[command_id];

      int next_idx = command_id + 1;

      // Used to see if we skip the next line if the current command is an if statement (true = run next, false = skip next)
      expression_flag = 1;

      // Get the command type
      int command_type = cur_command->command_type;

      // Execute the command
      switch (command_type)
      {
         case INT:
         {
            // Do nothing
            // All int variables are initialized in parse_int, we just go to the next command
            break;
         }
         case SET:
         {
            // Check if the variable is defined
            int index = is_defined(runtime, cur_command->args[0]);
            if (index == -1)
            {
               printf("Error at line %d: Variable %s is not defined\n", runtime->pc, cur_command->args[0]);
               return;
            }

            // Set the value of the variable
            runtime->intValues[index] = atoi(cur_command->args[1]);
            runtime->intValuesSet[index] = 1;
            runtime->intValuesLen = runtime->intValuesLen + 1;

            break;
         }
         case BEGIN:
         {
            // Do nothing, we just go to the next command
            break;
         }
         case END:
         {
            // Do nothing, we should never reach this point
            break;
         }
         case ADD:
         {
            // Check if the variable is set
            int index = is_set(runtime, cur_command->args[0]);
            if (index == -1)
            {
               printf("Error at line %d: Variable %s is not set\n", runtime->pc, cur_command->args[0]);
               return;
            }

            // Val_check = (-1 = not an int, 0 = positive integer, 1 = negative integer)
            int val_check = is_integer(cur_command->args[1]);
            int val = atoi(cur_command->args[1]);

            // If val_check is 1, then the value is negative
            if (val_check == 1)
            {
               val = -val;
            }
            // Add the value to the variable
            runtime->intValues[index] += atoi(cur_command->args[1]);
            break;
         }
         case SUB:
         {
            // Check if the variable is set
            int index = is_set(runtime, cur_command->args[0]);
            if (index == -1)
            {
               printf("Error at line %d: Variable %s is not set\n", runtime->pc, cur_command->args[0]);
               return;
            }

            // Check value to see if it is negative
            int val_check = is_integer(cur_command->args[1]);
            int val = atoi(cur_command->args[1]);

            // If val_check is 2, then the value is negative
            if (val_check == 2)
            {
               val = -val;
            }
            // Subtract the value from the variable
            runtime->intValues[index] -= atoi(cur_command->args[1]);
            break;
         }
         case MULT:
         {
            // Check if the variable is set
            int index = is_set(runtime, cur_command->args[0]);
            if (index == -1)
            {
               printf("Error at line %d: Variable %s is not set\n", runtime->pc, cur_command->args[0]);
               return;
            }

            int val_check = is_integer(cur_command->args[1]);
            int val = atoi(cur_command->args[1]);

            // If val_check is 2, then the value is negative
            if (val_check == 2)
            {
               val = -val;
            }
            // Multiply the variable by the value
            runtime->intValues[index] *= atoi(cur_command->args[1]);
            break;
         }
         case DIV:
         {
            // Check if the variable is set
            int index = is_set(runtime, cur_command->args[0]);
            if (index == -1)
            {
               printf("Error at line %d: Variable %s is not set\n", runtime->pc, cur_command->args[0]);
               return;
            }

            int val_check = is_integer(cur_command->args[1]);
            int val = atoi(cur_command->args[1]);

            // If val_check is 2, then the value is negative
            if (val_check == 2)
            {
               val = -val;
            }
            // Divide the variable by the value
            runtime->intValues[index] /= atoi(cur_command->args[1]);
            break;
         }
         case PRINT:
         {
            // Check if the variable is set
            int index1 = is_set(runtime, cur_command->args[0]);
            if (index1 == -1)
            {
               printf("Error at line %d: Variable %s is not set\n", runtime->pc, cur_command->args[0]);
               return;
            }

            // Check if the variable is set
            int index2 = is_set(runtime, cur_command->args[1]);
            if (index2 == -1)
            {
               printf("Error at line %d: Variable %s is not set\n", runtime->pc, cur_command->args[1]);
               return;
            }

            #ifndef NOGRAPHICS
            // Print the string to the screen at the specified coordinates (row, col)
            print(runtime->intValues[index1], runtime->intValues[index2], cur_command->args[2]);
            #else
            // Print the variable values
            printf("%d %d %s\n", runtime->intValues[index1], runtime->intValues[index2], cur_command->args[2]);
            #endif

            break;
         }
         case GOTO:
         {
            // Check if the line number is valid
            int line_number = atoi(cur_command->args[0]);
            if (line_number < runtime->begin_line || line_number > runtime->end_line)
            {
               printf("Error at line %d: Invalid line number %d\n", runtime->pc, line_number);
               return;
            }

            // Set next_idx to the index of the command with the line number
            int id = get_command_by_line_number(runtime, line_number);
            if (id == -1)
            {
               printf("Error: Command at line %d not found\n", line_number);
               return;
            }
            next_idx = id;
            break;
         }
         case IF:
         {
            int val1, val2;

            // Check if the variable is set
            int index1 = is_set(runtime, cur_command->args[0]);
            if (index1 == -1)
            {
               // Check if the value is an integer
               int is_int = is_integer(cur_command->args[0]);
               if (is_int == -1)
               {
                  printf("Error at line %d: %s is not defined\n", runtime->pc, cur_command->args[0]);
                  return;
               }
               else
               {
                  // Set val1 to the integer value
                  val1 = atoi(cur_command->args[0]);

                  // Check if the value is negative, if so apply the negative sign
                  if (is_int == 2)
                  {
                     val1 = -val1;
                  }
               }
            }
            else
            {
               // Set val1 to the value of the variable
               val1 = runtime->intValues[index1];
            }

            // Check if the variable is set
            int index2 = is_set(runtime, cur_command->args[2]);
            if (index2 == -1)
            {
               // Check if the value is an integer
               int is_int = is_integer(cur_command->args[2]);
               if (is_int == -1)
               {
                  printf("Error at line %d: %s is not defined\n", runtime->pc, cur_command->args[2]);
                  return;
               }
               else
               {
                  // Set val2 to the integer value
                  val2 = atoi(cur_command->args[2]);

                  // Check if the value is negative, if so apply the negative sign
                  if (is_int == 2)
                  {
                     val2 = -val2;
                  }
               }
            }
            else
            {
               // Set val2 to the value of the variable
               val2 = runtime->intValues[index2];
            }

            // If the expression is false, skip the next line
            if(evaluate(runtime, command_id, cur_command->args[1], val1, val2) == 0)
               next_idx++;
         }
      }

      // Go to the next command
      runtime->pc = runtime->commands[next_idx]->line_number;
   }

   return;
}

// Free the runtime structure
void free_runtime(Runtime *runtime)
{
   if (runtime == NULL)
   {
      return;
   }

   // Free the array of commands
   for (int i = 0; i < runtime->commandsLen; i++)
   {
      int command_type = runtime->commands[i]->command_type;

      // Free malloc'd char pointer for INT, SET, ADD, SUB, MULT, DIV, PRINT, IF
      if (command_type == INT || command_type == SET || command_type == ADD || command_type == SUB || command_type == MULT || command_type == DIV || command_type == PRINT || command_type == IF || command_type == GOTO)
      {
         free(runtime->commands[i]->args[0]);
      }

      // Free malloc'd char pointer for SET, ADD, SUB, MULT, DIV, PRINT, IF
      if (command_type == SET || command_type == ADD || command_type == SUB || command_type == MULT || command_type == DIV || command_type == PRINT || command_type == IF)
      {
         free(runtime->commands[i]->args[1]);
      }

      // Free malloc'd char pointer for PRINT, IF
      if (command_type == PRINT || command_type == IF)
      {
         free(runtime->commands[i]->args[2]);
      }

      // Free command
      free(runtime->commands[i]);
   }
   free(runtime->commands);

   // Free the runtime structure
   free(runtime);
}

// Print the runtime structure
void print_runtime(Runtime *runtime)
{
   if (runtime == NULL)
   {
      return;
   }

   printf("Index\t\tLine Number\t\tCommand\t\tArg1\t\tArg2\t\tArg3\n");

   // Print the array of commands
   for (int i = 0; i < runtime->commandsLen; i++)
   {  
      printf("%d\t\t", i);
      Command *command = runtime->commands[i];
      int command_type = command->command_type;
      printf("%d\t\t\t", command->line_number);
      printf("%s\t\t", command_type_to_string(command_type));
      if (command_type == INT || command_type == SET || command_type == ADD || command_type == SUB || command_type == MULT || command_type == DIV || command_type == PRINT || command_type == IF || command_type == GOTO)
         printf("%s\t\t", command->args[0]);
      if (command_type == SET || command_type == ADD || command_type == SUB || command_type == MULT || command_type == DIV || command_type == PRINT || command_type == IF)
         printf("%s\t\t", command->args[1]);
      if (command_type == PRINT || command_type == IF)
         printf("%s\t\t", command->args[2]);

      printf("\n");
   }
}

# pragma endregion

# pragma region Parser Functions

// Parse the line
int parse_line(Runtime *runtime, int n, char *line)
{
   // Check if the line is NULL
   if (line == NULL)
   {
      return -1;
   }

   int i = 0;

   // Get tokens from the line
   char *token = strtok(line, " ");

   // Initialize line number
   int line_number;

   // Reference to the command
   Command *command = runtime->commands[n];

   // Iterate through the tokens
   while (token != NULL)
   {
      // Remove the newline character from the token
      if (token[strlen(token) - 1] == '\n')
      {
         token[strlen(token) - 1] = '\0';
      }

      // Check if the token is not a space or newline
      if (isspace(*token) || *token == '\n')
         continue;

      // Determine the line number of the command
      if (i == 0)
      {
         // Check if the line number is an integer
         int is_int = is_integer(token);

         if (is_int == -1)
         {
            printf("Error at line %d: %s is not an integer\n", n, token);
            return -1;
         }
         if (is_int == 2)
         {
            printf("Error at line %d: %s is not a positive integer\n", n, token);
            return -1;
         }

         // Convert the line number to an integer
         line_number = atoi(token);
         command->line_number = line_number;
      }
      // Determine the type of command
      else if (i == 1)
      {
         int command_type = determine_command_type(token);
         if (command_type == -1)
         {
            printf("Error at line %d: Invalid command\n", line_number);
            return -1;
         }
         command->command_type = command_type;

         // Set begin and end line numbers
         if (command_type == BEGIN)
         {
            runtime->begin_line = line_number;
            runtime->begin_flag = 1;
         }
         else if (command_type == END)
         {
            runtime->end_line = line_number;
            runtime->end_flag = 1;
         }
      }
      // Determine the arguments for the command
      else
      {
         // Parse the arguments (subtracts 2 from i since the first two tokens are the line number and command type)
         parse_arg(runtime, n, token, i - 2);
      }

      // Get the next token
      token = strtok(NULL, " ");
      i++;
   }

   // Check if command has the correct number of arguments
   if (check_arguments(command->command_type, i - 2, line_number) == -1)
      return -1;

   // If the command is an int command we have to add the int name to the array of int names and increment the length
   if (command->command_type == INT)
      runtime->intNames[runtime->intNamesLen++] = command->args[0];

   return 1;
}

// Parse an argument depending on the command type and the argument index (i)
int parse_arg(Runtime *runtime, int n, char *token, int i)
{
   if (runtime == NULL)
   {
      return -1;
   }

   int line_number = runtime->commands[n]->line_number;
   int command_type = runtime->commands[n]->command_type;

   if (command_type == INT)
   {
      // Check if the variable name is too long
      if (strlen(token) > MAXVARNAME + 1)
      {
         printf("Error at line %d: Variable name %s is too long\n", line_number, token);
         return -1;
      }

      // Check if the variable is already defined
      if (is_defined(runtime, token) >= 0)
      {
         printf("Error at line %d: Variable %s is already defined\n", line_number, token);
         return -1;
      }

      // We have to make a copy of the variable name since strtok pointer is ever changing
      char *var_name = malloc(sizeof(char) * (strlen(token) + 1));
      strcpy(var_name, token);

      // Set arg1
      runtime->commands[n]->args[i] = var_name;

      return 1;
   }
   else if (command_type == SET || command_type == ADD || command_type == SUB || command_type == MULT || command_type == DIV)
   {
      // Handle the first argument
      if (i == 0)
      {
         // Check if the variable name is too long
         if (strlen(token) > MAXVARNAME + 1)
         {
            printf("Error at line %d: Variable name %s is too long\n", line_number, token);
            return -1;
         }

         // Check if the variable is already defined
         if (is_defined(runtime, token) == -1)
         {
            printf("Error at line %d: Variable %s is not defined\n", line_number, token);
            return -1;
         }

         // We have to make a copy of the variable name since strtok pointer is ever changing
         char *var_name = malloc(sizeof(char) * (strlen(token) + 1));
         strcpy(var_name, token);

         // Set arg1
         runtime->commands[n]->args[i] = var_name;
      }
      // Handle the second argument
      else if (i == 1)
      {
         // Check if value is an integer
         int is_int = is_integer(token);

         if (is_int == -1)
         {
            // Check if the value is a variable
            if (is_defined(runtime, token) == -1)
            {
               printf("Error at line %d: %s is not defined\n", n, token);
               return -1;
            }
         }

         // Write copy of token to args[1]
         char *val = malloc(sizeof(char) * strlen(token) + 1);
         strcpy(val, token);

         runtime->commands[n]->args[i] = val;
      }

      return 1;
   }
   else if (command_type == PRINT)
   {
      // Handle the first argument
      if (i == 0)
      {
         // Check if the variable name is too long
         if (strlen(token) > MAXVARNAME + 1)
         {
            printf("Error at line %d: Variable name %s is too long\n", line_number, token);
            return -1;
         }

         // Check if the variable is already defined
         if (is_defined(runtime, token) == -1)
         {
            printf("Error at line %d: Variable %s is not defined\n", line_number, token);
            return -1;
         }

         // We have to make a copy of the variable name since strtok pointer is ever changing
         char *var_name = malloc(sizeof(char) * (strlen(token) + 1));
         strcpy(var_name, token);

         // Set arg1
         runtime->commands[n]->args[i] = var_name;
      }
      // Handle the second argument
      else if (i == 1)
      {
         // Check if the variable name is too long
         if (strlen(token) > MAXVARNAME + 1)
         {
            printf("Error at line %d: Variable name %s is too long\n", line_number, token);
            return -1;
         }

         // Check if the variable is already defined
         if (is_defined(runtime, token) == -1)
         {
            printf("Error at line %d: Variable %s is not defined\n", line_number, token);
            return -1;
         }

         // We have to make a copy of the variable name since strtok pointer is ever changing
         char *var_name = malloc(sizeof(char) * (strlen(token) + 1));
         strcpy(var_name, token);

         // Set arg1
         runtime->commands[n]->args[i] = var_name;
      }
      // Handle the third argument
      else if (i == 2)
      {
         // Write copy of token to args[2], this token cannot contain spaces since it was tokenized by spaces
         char *val = malloc(sizeof(char) * strlen(token) + 1);
         strcpy(val, token);

         runtime->commands[n]->args[i] = val;
      }

      return 1;
   }
   else if (command_type == GOTO)
   {
      int is_int = is_integer(token);

      if (is_int == -1)
      {
         printf("Error at line %d: %s is not an integer\n", n, token);
         return -1;
      }

      if (is_int == 2)
      {
         printf("Error at line %d: %s is not a positive integer\n", n, token);
         return -1;
      }

      // We have to make a copy of the variable name since strtok pointer is ever changing
      char *val = malloc(sizeof(char) * (strlen(token) + 1));
      strcpy(val, token);

      // Set arg1
      runtime->commands[n]->args[i] = val;

      return 1;
   }
   else if (command_type == IF)
   {
      // Handle the first argument
      if (i == 0)
      {
         // Check if the variable name is too long
         if (strlen(token) > MAXVARNAME + 1)
         {
            printf("Error at line %d: Variable name %s is too long\n", line_number, token);
            return -1;
         }

         // We have to make a copy of the variable name since strtok pointer is ever changing
         char *var_name = malloc(sizeof(char) * (strlen(token) + 1));
         strcpy(var_name, token);

         // Set arg1
         runtime->commands[n]->args[i] = var_name;
      }
      // Handle the second argument
      else if (i == 1)
      {
         // We have to make a copy of the variable name since strtok pointer is ever changing
         char *op = malloc(sizeof(char) * (strlen(token) + 1));
         strcpy(op, token);

         // Check if the operator is valid
         if (strcmp(op, "eq") == 0 || strcmp(op, "ne") == 0 || strcmp(op, "gt") == 0 || strcmp(op, "gte") == 0 || strcmp(op, "lt") == 0 || strcmp(op, "lte") == 0)
         {
            // Set arg1
            runtime->commands[n]->args[i] = op;
         }
         else
         {
            printf("Error at line %d: Invalid operator %s\n", line_number, op);
            return -1;
         }
      }
      // Handle the third argument
      else if (i == 2)
      {
         // Check if the variable name is too long
         if (strlen(token) > MAXVARNAME + 1)
         {
            printf("Error at line %d: Variable name %s is too long\n", line_number, token);
            return -1;
         }

         // We have to make a copy of the variable name since strtok pointer is ever changing
         char *var_name = malloc(sizeof(char) * (strlen(token) + 1));
         strcpy(var_name, token);

         // Set arg2
         runtime->commands[n]->args[i] = var_name;
      }

      return 1;
   }
   else
   {
      printf("Error at line %d: Invalid command\n", line_number);
      return -1;
   }
}

# pragma endregion

# pragma region Helper Functions

// Check if the variable is defined, return the index of the variable if it is defined
int is_defined(Runtime *runtime, char *arg)
{
   // Check if the variable is defined
   for (int i = 0; i < runtime->intNamesLen; i++)
   {
      // Check if the variable is defined
      char *v = runtime->intNames[i];
      if (strcmp(v, arg) == 0)
      {
         return i;
      }
   }
   return -1;
}

// Check if the value of the variable is set, and return the index of the variable
int is_set(Runtime *runtime, char *arg)
{
   // Check if the variable is defined
   for (int i = 0; i < runtime->intValuesLen; i++)
   {
      // Check if the variable is defined
      char *v = runtime->intNames[i];
      if (is_defined(runtime, v) >= 0)
      {
         // Check if the defined variable is the same as the variable we are checking
         if (strcmp(v, arg) == 0)
         {
            // Check if the variable is set
            if (runtime->intValuesSet[i] == 1)
            {
               return i;
            }
            else
            {
               return -1;
            }
         }
      }
   }

   // Variable is not set
   return -1;
}

// Evaluate an expression
int evaluate(Runtime *runtime, int n, char *op, int val1, int val2)
{
   if (strcmp(op, "eq") == 0)
   {
      return (val1 == val2);
   }
   else if (strcmp(op, "ne") == 0)
   {
      return (val1 != val2);
   }
   else if (strcmp(op, "gt") == 0)
   {
      return (val1 > val2);
   }
   else if (strcmp(op, "gte") == 0)
   {
      return (val1 >= val2);
   }
   else if (strcmp(op, "lt") == 0)
   {
      return (val1 < val2);
   }
   else if (strcmp(op, "lte") == 0)
   {
      return (val1 <= val2);
   }
   else
   {
      return -1;
   }
}

// Check if the number of arguments is correct for the command
int check_arguments(int command_type, int argc, int line_number)
{
   // Check to see if the correct number of arguments are provided
   switch (command_type)
   {
   case INT:
      if (argc != 1)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'int'\n\tint <var>\n", line_number);
         return -1;
      }
      break;
   case SET:
      if (argc != 2)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'set'\n\tset <var> #\n", line_number);
         return -1;
      }
      break;
   case BEGIN:
      if (argc != 0)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'begin'\n\tbegin\n", line_number);
         return -1;
      }
      break;
   case END:
      if (argc != 0)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'end'\n\tend\n", line_number);
         return -1;
      }
      break;
   case ADD:
      if (argc != 2)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'add'\n\tadd <var> #\n", line_number);
         return -1;
      }
      break;
   case SUB:
      if (argc != 2)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'sub'\n\tsub <var> #\n", line_number);
         return -1;
      }
      break;
   case MULT:
      if (argc != 2)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'mult'\n\tmult <var> #\n", line_number);
         return -1;
      }
      break;
   case DIV:
      if (argc != 2)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'div'\n\tdiv <var> #\n", line_number);
         return -1;
      }
      break;
   case PRINT:
      if (argc != 3)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'print'\n\tprint <var1> <var2> string\n", line_number);
         return -1;
      }
      break;
   case GOTO:
      if (argc != 1)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'goto'\n\tgoto <lineNumber>\n", line_number);
         return -1;
      }
      break;
   case IF:
      if (argc != 3)
      {
         printf("Error at line %d: Incorrect number of arguments for command 'if'\n\tif <var> <op> <var>\n", line_number);
         return -1;
      }
      break;
   default:
      printf("Error at line %d: Invalid command\n", line_number);
      return -1;
   }
}

// Determine the syntax flag of the command
int determine_command_type(char *token)
{
   // Check if the command is a valid command
   if (strcmp(token, "int") == 0)
   {
      return INT;
   }
   else if (strcmp(token, "set") == 0)
   {
      return SET;
   }
   else if (strcmp(token, "begin") == 0)
   {
      return BEGIN;
   }
   else if (strcmp(token, "end") == 0)
   {
      return END;
   }
   else if (strcmp(token, "add") == 0)
   {
      return ADD;
   }
   else if (strcmp(token, "sub") == 0)
   {
      return SUB;
   }
   else if (strcmp(token, "mult") == 0)
   {
      return MULT;
   }
   else if (strcmp(token, "div") == 0)
   {
      return DIV;
   }
   else if (strcmp(token, "print") == 0)
   {
      return PRINT;
   }
   else if (strcmp(token, "goto") == 0)
   {
      return GOTO;
   }
   else if (strcmp(token, "if") == 0)
   {
      return IF;
   }
   else
   {
      return -1;
   }
}

// Return the command with the given line number
int get_command_by_line_number(Runtime *runtime, int line_number)
{
   if (runtime == NULL)
   {
      return -1;
   }

   // Loop through the commands
   for (int i = 0; i < runtime->commandsLen; i++)
   {
      // Check if the line number matches
      if (runtime->commands[i]->line_number == line_number)
      {
         return i;
      }
   }

   // Line number not found
   return -1;
}

// Return string representation of the command
const char *command_type_to_string(int command_type)
{
   switch (command_type)
   {
   case INT:
      return "int";
   case SET:
      return "set";
   case BEGIN:
      return "begin";
   case END:
      return "end";
   case ADD:
      return "add";
   case SUB:
      return "sub";
   case MULT:
      return "mult";
   case DIV:
      return "div";
   case PRINT:
      return "print";
   case GOTO:
      return "goto";
   case IF:
      return "if";
   default:
      return "invalid";
   }
}

// Check if the value is an integer (-1 = not an integer, 0 = positive integer, 1 = negative integer)
int is_integer(char *token)
{
   int negative = 0;

   for (int i = 0; i < strlen(token); i++)
   {
      // Check if the value is a negative number
      if (i == 0 && token[i] == '-')
      {
         negative = 1;
         continue;
      }

      // Check if the value is actually a number
      if (!isdigit(token[i]))
      {
         return -1;
      }
   }

   return negative;
}

# pragma endregion