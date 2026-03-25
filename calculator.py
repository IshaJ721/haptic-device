from importlib import import_module
from ast import literal_eval

from ase import Atoms


def _resolve_calculator(spec):
    if not spec or spec in {"lj", "lennard-jones"}:
        module_name = "ase.calculators.lj"
        class_name = "LennardJones"
        kwargs = {}
    elif spec == "morse":
        module_name = "ase.calculators.morse"
        class_name = "MorsePotential"
        kwargs = {}
    elif spec == "emt":
        module_name = "ase.calculators.emt"
        class_name = "EMT"
        kwargs = {}
    else:
        parts = spec.split(":", 2)
        if len(parts) < 2:
            raise ValueError(
                "Calculator spec must be empty, a known alias, or module:Class[:kwargs]"
            )
        module_name, class_name = parts[0], parts[1]
        kwargs = literal_eval(parts[2]) if len(parts) == 3 else {}
        if not isinstance(kwargs, dict):
            raise ValueError("Calculator kwargs must evaluate to a dict")

    calculator_class = getattr(import_module(module_name), class_name)
    return calculator_class(**kwargs)


def get_values(numbers, positions, cell=None, pbc=None, calculator_spec=""):
    atoms = Atoms(numbers=numbers, positions=positions, cell=cell, pbc=pbc)
    atoms.calc = _resolve_calculator(calculator_spec)
    return {
        "forces": atoms.get_forces().tolist(),
        "energy": atoms.get_potential_energy(),
    }


if __name__ == "__main__":
    sample = get_values(numbers=[78, 78], positions=[[0, 0, 0], [2.8, 0, 0]])
    print(sample)
