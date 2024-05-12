import csv
import re


def parse_output(file_path):
    # Regex pattern to extract details
    pattern = re.compile(
        r"Results for distribution '(\w+)' with (\d+) rows and (\d+) columns\.\n\n"
        r"Construction Time: ([\d.]+) seconds or ([\d.]+) microseconds / key\n"
        r"File Size: (\d+) bytes or ([\d.]+) bytes / key\n"
        r"Average Query Time: ([\d.]+) ns"
    )

    results = []
    with open(file_path, "r") as file:
        data = file.read()

    # Find all matches and extract the data
    matches = pattern.finditer(data)
    for match in matches:
        result = {
            "Distribution": match.group(1),
            "Rows": int(match.group(2)),
            "Columns": int(match.group(3)),
            "Construction Time (s)": float(match.group(4)),
            "Construction Time (us/key)": float(match.group(5)),
            "File Size (bytes)": int(match.group(6)),
            "File Size (bytes/key)": float(match.group(7)),
            "Average Query Time (ns)": float(match.group(8)),
        }
        results.append(result)

    return results


def write_to_csv(results, output_csv_path):
    # Define the headers for the CSV file
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

    # Writing to CSV
    with open(output_csv_path, "w", newline="") as csvfile:
        writer = csv.DictWriter(csvfile, fieldnames=headers)
        writer.writeheader()
        for data in results:
            writer.writerow(data)


# Example usage
file_path = (
    "outputs.txt"  # copy paste outputs of hyperparameter_search.py to outputs.txt
)
output_csv_path = "results.csv"
parsed_results = parse_output(file_path)
write_to_csv(parsed_results, output_csv_path)
print(f"Results have been written to {output_csv_path}")
