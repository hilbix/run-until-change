# run-until-change

- Runs a command or script
- If the program terminates, return the normal status code
- If one of the given files change, kill the program
- If no file is given, use the command or script name is checked

Currently, the checks are purely `stat()` based:

- All given files must exist when `run-until-change` starts.
- Checks are done each second.
- If a checked file vanishes, this is ignored
  - If a vanished file reappears, it is checked again

Future thoughts:

- If you give a directory, use `fanotify()` to detected changes in the given subpath


## Usage

	git clone https://github.com/hilbix/run-until-change.git
	cd run-until-change
	make
	sudo make install

Then:

	run-until-change files_to_check.. -- command_or_script [args..]


## Example

Typical use while developing a script:

	while date && ! read -t.5; do run-until-change ./script.py -- python3vim.sh ./script.py; done

- [Example for python3vim.sh](https://github.com/hilbix/tino/blob/master/howto/python3vim.sh)

