from ase.io import read


def get_state_information(filename):
    atoms = read(filename)
    return {
        "positions": atoms.get_positions().tolist(),
        "atomic_numbers": atoms.get_atomic_numbers().tolist(),
        "cell": atoms.get_cell()[:].tolist(),
        "pbc": atoms.get_pbc().tolist(),
    }


if __name__ == "__main__":
    import sys

    if len(sys.argv) != 2:
        raise SystemExit("usage: python ase_file_io.py <structure-file>")

    info = get_state_information(sys.argv[1])
    print(len(info["positions"]))
    for position in info["positions"]:
        print(f"{position[0]} {position[1]} {position[2]}")
    print(" ".join(str(number) for number in info["atomic_numbers"]))
    for row in info["cell"]:
        print(" ".join(str(value) for value in row))
    print(" ".join("1" if flag else "0" for flag in info["pbc"]))
