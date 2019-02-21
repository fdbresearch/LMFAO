from argparse import ArgumentParser
from pathlib import Path

TAB_SIZE = 3


def read_and_parse_path(data_path: str, output_path: str, one_line: bool):
    with open (data_path) as file_input:
        lines = file_input.readlines()
        with open(output_path, 'w') as file_output:
            file_output.write("\"\"")
            for line in lines:
                white_spaces_no = len(line) - len(line.lstrip())
                tab_no = int(white_spaces_no / TAB_SIZE)
                line = line.replace("\"", "\\\"")
                line_new = "+ offset(" + str(tab_no) + ") + " + "\"" + line.lstrip()[:-1] + "\\n\""
                if not one_line:
                    line_new += "\n"
                file_output.write(line_new)


if __name__ == "__main__":
    parser = ArgumentParser()
    parser.add_argument("-o", "--output_path", dest="output_path", required=True)
    parser.add_argument("-d", "--data_path", dest="data_path", required=True)
    parser.add_argument("-ol", "--one_line", type=bool, default=False,  nargs='?', const=True)

    args = parser.parse_args()
    output_path = args.output_path
    data_path = args.data_path
    one_line = args.one_line
    read_and_parse_path(data_path, output_path, one_line)