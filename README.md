# Shell-Alike-Application

This program implements a shell-alike application, which acts like the Terminal / Konsole application in Ubuntu.

FUNCTIONALITIES: The following commands are included, along with their parameters:

1. The "cat" command. Parameters implemented are: -b -E -n -s; also implemented all possible combinations
2. The "head" command. Parameters implemented are: -c, -n, -q, -v; also implemented all possible combinations
3. The "env" command. Parameters implemented are: -u
4. The program also supports pipes in the commands, e.g. > ls -l | grep -e 'tmp' | wc -l (it works with any type of command)
5. The program also supports redirection in the commands, e.g. > ls -l > out.txt (it works with any type of command)
6. The program supports both pipes and redirection in the same command line, e.g. > ls -l | grep -e 'tmp' > out.txt (it works with any type of command)

Additional functionalities:
- history for previously used commands
- types of redirection: >, >>, <, << 
      - all of them work for all possible combinations for the function "cat"
      - only >, >> and < work for all "head" parameters; they are not fully implemented for combinations in "head" 
      
- possible combinations supported (IMPORTANT! YOU CAN REPLACE THE FILE NAMES WITH ANY FILE YOU HAVE, THESE ARE JUST THE ONES THAT I USED):

      - cat 
      - cat textfile.txt
      - cat -
      - cat textfile.txt numbers.txt ... (works on any number of files)
      - cat textfile.txt - numbers.txt (works for only 1 more file after the standard input; also, works for only one such standard input)
      - cat textfile.txt -
      - all combinations above work for >, >>, <, << redirection types
     
      - head
      - head textfile.txt
      - head -
      - head textfile.txt numbers.txt ... (works on ant number of files)
      - all combinations above work for >, >>, < redirection types
      - head also works on standard input (-), but does not have the best results and can be glitchy

      - env
      - env -u FIELDNAME (you can see all possible fieldnames by executing simple env, they are the ones in capital letters at the beginning of the lines)
      - env -u FIELDNAME cat ... (works with any mentioned combination for cat)
      - env -u FIELDNAME head ... (works with any mentioned combination for head)
