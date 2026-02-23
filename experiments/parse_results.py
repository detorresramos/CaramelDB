import csv
import re


def parse_output(file_path):
    pattern = re.compile(
        r"Results for distribution '(\w+)' with (\d+) rows and (\d+) columns\.\n\n"
        r"Construction Time: ([\d.]+) seconds or ([\d.]+) microseconds / key\n"
        r"File Size: (\d+) bytes or ([\d.]+) bytes / key\n"
        r"Average Query Time: ([\d.]+) ns"
    )

    with open(file_path, "r") as file:
        data = file.read()

    results = []
    for match in pattern.finditer(data):
        results.append(
            {
                "Distribution": match.group(1),
                "Rows": int(match.group(2)),
                "Columns": int(match.group(3)),
                "Construction Time (s)": float(match.group(4)),
                "Construction Time (us/key)": float(match.group(5)),
                "File Size (bytes)": int(match.group(6)),
                "File Size (bytes/key)": float(match.group(7)),
                "Average Query Time (ns)": float(match.group(8)),
            }
        )

    return results


def write_to_csv(results, output_csv_path):
    headers = [
        "Distribution",
        "Rows",
        "Columns",
        "Construction Time (s)",
        "Construction Time (us/key)",
        "File Size (bytes)",
        "File Size (bytes/key)",
        "Average Query Time (ns)",
    ]

    with open(output_csv_path, "w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=headers)
        writer.writeheader()
        for data in results:
            writer.writerow(data)


if __name__ == "__main__":
    file_path = "outputs.txt"
    output_csv_path = "results.csv"
    parsed_results = parse_output(file_path)
    write_to_csv(parsed_results, output_csv_path)
    print(f"Results have been written to {output_csv_path}")
