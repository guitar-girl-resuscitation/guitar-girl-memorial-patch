"""Print Java literals for the startup banner; development-only pyfiglet 1.0.4.

Run with pyfiglet on PYTHONPATH. No font, generator, or runtime dependency is
shipped in the APK. Review the output before updating TerminalText.java.
"""
import json

from pyfiglet import Figlet


def main() -> None:
    rows = Figlet(font="standard", width=200).renderText("AIRISUTEK").rstrip().splitlines()
    for index, row in enumerate(rows):
        suffix = ";" if index == len(rows) - 1 else " +"
        print(json.dumps(row.rstrip() + ("\n\n" if index == len(rows) - 1 else "\n")) + suffix)


if __name__ == "__main__":
    main()
