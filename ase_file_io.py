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

    print(get_state_information(sys.argv[1]))
