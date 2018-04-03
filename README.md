# run-until-change

- Runs a command or script
- If the program terminates, return the normal status code
- If one of the given files change, kill the program (see list of signals below)
- If no file is given, use the command or script name is checked
- For proper signal processing, the child is run under `setsid()`
  - Signals sent to `run-until-change` are forwarded to the complete session
  - This way, Ctrl+C works as expected.
- You can give a list of signals on the commandline.  These are sent with 1s delay.
- The default is to SIGTERM then SIGKILL.  (SIGKILL is always the last signal)

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
	git submodule update --init
	make
	sudo make install

Then:

	run-until-change [-signal..] files_to_check.. -- command_or_script [args..]


## Example

Typical use while developing a script:

	while date && ! read -t.5; do run-until-change ./script.py -- python3vim.sh ./script.py; done

- [Example for python3vim.sh](https://github.com/hilbix/tino/blob/master/howto/python3vim.sh)

