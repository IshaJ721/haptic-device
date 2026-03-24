#include "potentials.h"

#include <Python.h>

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#include "atom.h"

extern double centerCoords[3];

namespace {

constexpr double kDistanceScale = 0.02;
constexpr double kDefaultCellLength = 100.0;

[[noreturn]] void failWithPythonError(const char* message) {
  PyErr_Print();
  std::cerr << message << std::endl;
  std::exit(1);
}

std::vector<double> flattenPositions(const std::vector<Atom*>& spheres) {
  std::vector<double> positions;
  positions.reserve(spheres.size() * 3);

  for (const Atom* sphere : spheres) {
    cVector3d pos = sphere->getLocalPos();
    positions.push_back(pos.x() / kDistanceScale + centerCoords[0]);
    positions.push_back(pos.y() / kDistanceScale + centerCoords[1]);
    positions.push_back(pos.z() / kDistanceScale + centerCoords[2]);
  }

  return positions;
}

std::vector<int> collectAtomicNumbers(const std::vector<Atom*>& spheres) {
  std::vector<int> numbers;
  numbers.reserve(spheres.size());

  for (const Atom* sphere : spheres) {
    numbers.push_back(sphere->getAtomicNumber());
  }

  return numbers;
}

void parseCalculatorSpec(const std::string& spec,
                         std::string& moduleName,
                         std::string& className,
                         std::string& kwargsText) {
  if (spec.empty()) {
    moduleName = "ase.calculators.lj";
    className = "LennardJones";
    kwargsText.clear();
    return;
  }

  size_t firstColon = spec.find(':');
  if (firstColon == std::string::npos) {
    moduleName = spec;
    className = "LennardJones";
    kwargsText.clear();
    return;
  }

  moduleName = spec.substr(0, firstColon);
  size_t secondColon = spec.find(':', firstColon + 1);
  if (secondColon == std::string::npos) {
    className = spec.substr(firstColon + 1);
    kwargsText.clear();
    return;
  }

  className = spec.substr(firstColon + 1, secondColon - firstColon - 1);
  kwargsText = spec.substr(secondColon + 1);
}

PyObject* importModule(const char* moduleName) {
  PyObject* module = PyImport_ImportModule(moduleName);
  if (module == nullptr) {
    failWithPythonError("Failed to import a required Python module.");
  }
  return module;
}

PyObject* getCallable(PyObject* module, const char* attributeName) {
  PyObject* callable = PyObject_GetAttrString(module, attributeName);
  if (callable == nullptr || !PyCallable_Check(callable)) {
    Py_XDECREF(callable);
    Py_DECREF(module);
    failWithPythonError("Failed to resolve a required Python callable.");
  }
  return callable;
}

PyObject* buildNumbersList(const std::vector<int>& atomicNumbers) {
  PyObject* numbers = PyList_New(atomicNumbers.size());
  if (numbers == nullptr) {
    failWithPythonError("Failed to allocate Python list for atomic numbers.");
  }

  for (Py_ssize_t index = 0; index < static_cast<Py_ssize_t>(atomicNumbers.size()); ++index) {
    PyObject* value = PyLong_FromLong(atomicNumbers[index]);
    if (value == nullptr) {
      Py_DECREF(numbers);
      failWithPythonError("Failed to convert atomic number for Python.");
    }
    PyList_SetItem(numbers, index, value);
  }

  return numbers;
}

PyObject* buildPositionsList(const std::vector<double>& positions) {
  PyObject* rows = PyList_New(positions.size() / 3);
  if (rows == nullptr) {
    failWithPythonError("Failed to allocate Python list for positions.");
  }

  for (Py_ssize_t atomIndex = 0; atomIndex < static_cast<Py_ssize_t>(positions.size() / 3);
       ++atomIndex) {
    PyObject* row = PyList_New(3);
    if (row == nullptr) {
      Py_DECREF(rows);
      failWithPythonError("Failed to allocate Python position row.");
    }

    for (Py_ssize_t coordIndex = 0; coordIndex < 3; ++coordIndex) {
      PyObject* value =
          PyFloat_FromDouble(positions[static_cast<size_t>(atomIndex * 3 + coordIndex)]);
      if (value == nullptr) {
        Py_DECREF(row);
        Py_DECREF(rows);
        failWithPythonError("Failed to convert position value for Python.");
      }
      PyList_SetItem(row, coordIndex, value);
    }

    PyList_SetItem(rows, atomIndex, row);
  }

  return rows;
}

PyObject* buildCellList() {
  PyObject* cell = PyList_New(3);
  if (cell == nullptr) {
    failWithPythonError("Failed to allocate Python list for cell.");
  }

  for (Py_ssize_t index = 0; index < 3; ++index) {
    PyObject* value = PyFloat_FromDouble(kDefaultCellLength);
    if (value == nullptr) {
      Py_DECREF(cell);
      failWithPythonError("Failed to convert cell value for Python.");
    }
    PyList_SetItem(cell, index, value);
  }

  return cell;
}

PyObject* buildPbcList() {
  PyObject* pbc = PyList_New(3);
  if (pbc == nullptr) {
    failWithPythonError("Failed to allocate Python list for PBC.");
  }

  for (Py_ssize_t index = 0; index < 3; ++index) {
    PyObject* value = PyBool_FromLong(0);
    if (value == nullptr) {
      Py_DECREF(pbc);
      failWithPythonError("Failed to convert PBC value for Python.");
    }
    PyList_SetItem(pbc, index, value);
  }

  return pbc;
}

PyObject* buildKwargsDict(const std::string& kwargsText) {
  if (kwargsText.empty()) {
    return PyDict_New();
  }

  PyObject* astModule = importModule("ast");
  PyObject* literalEval = getCallable(astModule, "literal_eval");
  PyObject* kwargsString = PyUnicode_FromString(kwargsText.c_str());
  if (kwargsString == nullptr) {
    Py_DECREF(literalEval);
    Py_DECREF(astModule);
    failWithPythonError("Failed to convert ASE calculator kwargs for Python.");
  }

  PyObject* parsedKwargs = PyObject_CallFunctionObjArgs(literalEval, kwargsString, nullptr);
  Py_DECREF(kwargsString);
  Py_DECREF(literalEval);
  Py_DECREF(astModule);
  if (parsedKwargs == nullptr) {
    failWithPythonError("Failed to parse ASE calculator kwargs.");
  }
  if (!PyDict_Check(parsedKwargs)) {
    Py_DECREF(parsedKwargs);
    std::cerr << "ASE calculator kwargs must evaluate to a dict." << std::endl;
    std::exit(1);
  }

  return parsedKwargs;
}

std::vector<std::vector<double>> runAseCalculation(const std::vector<Atom*>& spheres,
                                                   const std::string& moduleName,
                                                   const std::string& className,
                                                   const std::string& kwargsText) {
  if (!Py_IsInitialized()) {
    Py_Initialize();
  }

  PyGILState_STATE gilState = PyGILState_Ensure();

  std::vector<int> atomicNumbers = collectAtomicNumbers(spheres);
  std::vector<double> positions = flattenPositions(spheres);

  PyObject* aseModule = importModule("ase");
  PyObject* atomsClass = getCallable(aseModule, "Atoms");

  PyObject* calcModule = importModule(moduleName.c_str());
  PyObject* calcClass = getCallable(calcModule, className.c_str());
  PyObject* calcArgs = PyTuple_New(0);
  PyObject* calcKwargs = buildKwargsDict(kwargsText);
  PyObject* calcObject = PyObject_Call(calcClass, calcArgs, calcKwargs);
  Py_DECREF(calcKwargs);
  Py_DECREF(calcArgs);
  Py_DECREF(calcClass);
  Py_DECREF(calcModule);
  if (calcObject == nullptr) {
    Py_DECREF(atomsClass);
    Py_DECREF(aseModule);
    failWithPythonError("Failed to construct the ASE calculator.");
  }

  PyObject* atomsArgs = PyTuple_New(0);
  PyObject* atomsKwargs = PyDict_New();
  if (atomsArgs == nullptr || atomsKwargs == nullptr) {
    Py_XDECREF(atomsArgs);
    Py_XDECREF(atomsKwargs);
    Py_DECREF(calcObject);
    Py_DECREF(atomsClass);
    Py_DECREF(aseModule);
    failWithPythonError("Failed to allocate ASE Atoms constructor arguments.");
  }

  PyObject* numbersObject = buildNumbersList(atomicNumbers);
  PyObject* positionsObject = buildPositionsList(positions);
  PyObject* cellObject = buildCellList();
  PyObject* pbcObject = buildPbcList();
  PyDict_SetItemString(atomsKwargs, "numbers", numbersObject);
  PyDict_SetItemString(atomsKwargs, "positions", positionsObject);
  PyDict_SetItemString(atomsKwargs, "cell", cellObject);
  PyDict_SetItemString(atomsKwargs, "pbc", pbcObject);
  Py_DECREF(numbersObject);
  Py_DECREF(positionsObject);
  Py_DECREF(cellObject);
  Py_DECREF(pbcObject);

  PyObject* atomsObject = PyObject_Call(atomsClass, atomsArgs, atomsKwargs);
  Py_DECREF(atomsKwargs);
  Py_DECREF(atomsArgs);
  Py_DECREF(atomsClass);
  Py_DECREF(aseModule);
  if (atomsObject == nullptr) {
    Py_DECREF(calcObject);
    failWithPythonError("Failed to construct ASE Atoms.");
  }

  if (PyObject_SetAttrString(atomsObject, "calc", calcObject) != 0) {
    Py_DECREF(calcObject);
    Py_DECREF(atomsObject);
    failWithPythonError("Failed to attach the ASE calculator to Atoms.");
  }
  Py_DECREF(calcObject);

  PyObject* forcesObject = PyObject_CallMethod(atomsObject, "get_forces", nullptr);
  if (forcesObject == nullptr) {
    Py_DECREF(atomsObject);
    failWithPythonError("Failed to evaluate ASE forces.");
  }

  PyObject* energyObject = PyObject_CallMethod(atomsObject, "get_potential_energy", nullptr);
  Py_DECREF(atomsObject);
  if (energyObject == nullptr) {
    Py_DECREF(forcesObject);
    failWithPythonError("Failed to evaluate ASE potential energy.");
  }

  PyObject* forceRows =
      PySequence_Fast(forcesObject, "ASE get_forces() result must be a sequence.");
  Py_DECREF(forcesObject);
  if (forceRows == nullptr) {
    Py_DECREF(energyObject);
    failWithPythonError("Failed to inspect ASE force rows.");
  }

  std::vector<std::vector<double>> returnVector;
  returnVector.reserve(spheres.size() + 1);
  PyObject** rowItems = PySequence_Fast_ITEMS(forceRows);
  for (Py_ssize_t atomIndex = 0; atomIndex < PySequence_Fast_GET_SIZE(forceRows); ++atomIndex) {
    PyObject* rowSequence =
        PySequence_Fast(rowItems[atomIndex], "Each ASE force row must be a sequence.");
    if (rowSequence == nullptr) {
      Py_DECREF(forceRows);
      Py_DECREF(energyObject);
      failWithPythonError("Failed to inspect ASE force components.");
    }

    PyObject** coordItems = PySequence_Fast_ITEMS(rowSequence);
    std::vector<double> pushBack = {PyFloat_AsDouble(coordItems[0]),
                                    PyFloat_AsDouble(coordItems[1]),
                                    PyFloat_AsDouble(coordItems[2])};
    returnVector.push_back(pushBack);
    Py_DECREF(rowSequence);
  }
  Py_DECREF(forceRows);

  returnVector.push_back({PyFloat_AsDouble(energyObject)});
  Py_DECREF(energyObject);
  PyGILState_Release(gilState);

  return returnVector;
}

}  // namespace

///////////////////////////// MORSE ///////////////////////////////////////
// Force and Potential energy calculation function for the Morse calculator
vector<vector<double>> morseCalculator::getFandU(vector<Atom*>& spheres) {
  vector<vector<double>> returnVector;
  double potentialEnergy = 0;
  Atom* current;
  for (int i = 0; i < spheres.size(); i++) {
    // compute force on atom
    cVector3d force;
    current = spheres[i];
    cVector3d pos0 = current->getLocalPos();
    // check forces with all other spheres
    force.zero();

    // this loop is for finding all of atom i's neighbors
    for (int j = 0; j < spheres.size(); j++) {
      // Don't compute forces between an atom and itself
      if (i != j) {
        // get position of sphere
        cVector3d pos1 = spheres[j]->getLocalPos();

        // compute direction vector from sphere 0 to 1

        cVector3d dir01 = cNormalize(pos0 - pos1);

        // compute distance between both spheres
        double distance = cDistance(pos0, pos1) / distanceScale;
        potentialEnergy += getMorseEnergy(distance);
        double appliedForce = getMorseForce(distance);
        force.add(appliedForce * dir01);
      }
    }
    vector<double> pushBack = {force.x(), force.y(), force.z()};
    returnVector.push_back(pushBack);
  }
  // Potential energy -- Halve it because pairwise
  vector<double> potentE = {potentialEnergy / 2};
  returnVector.push_back(potentE);

  return returnVector;
}

// Pairwise energy calculation for morse
double morseCalculator::getMorseEnergy(double distance) {
  double expf = exp(rho0 * (1.0 - distance / r0));
  return epsilon * expf * (expf - 2);
}

// Pairwise force calculation for morseCalculator
double morseCalculator::getMorseForce(double distance) {
  double temp = -2 * rho0 * epsilon * exp(rho0 - (2 * rho0 * distance) / r0) *
                (exp((rho0 * distance) / r0) - exp(rho0));
  return temp / r0 * forceDamping;
}

//////////////////////////////// LJ ///////////////////////////////////////
// Force and Potential energy calculation function for the lj calculator
vector<vector<double>> ljCalculator::getFandU(vector<Atom*>& spheres) {
  vector<vector<double>> returnVector;
  double potentialEnergy = 0;
  Atom* current;
  for (int i = 0; i < spheres.size(); i++) {
    // compute force on atom
    cVector3d force;
    current = spheres[i];
    cVector3d pos0 = current->getLocalPos();
    // check forces with all other spheres
    force.zero();

    // this loop is for finding all of atom i's neighbors
    for (int j = 0; j < spheres.size(); j++) {
      // Don't compute forces between an atom and itself
      if (i != j) {
        // get position of sphere
        cVector3d pos1 = spheres[j]->getLocalPos();

        // compute direction vector from sphere 0 to 1

        cVector3d dir01 = cNormalize(pos0 - pos1);

        // compute distance between both spheres
        double distance = cDistance(pos0, pos1) / distanceScale;
        potentialEnergy += getLennardJonesEnergy(distance);
        double appliedForce = getLennardJonesForce(distance);
        force.add(appliedForce * dir01);
      }
    }
    vector<double> pushBack = {force.x(), force.y(), force.z()};
    returnVector.push_back(pushBack);
  }
  // Potential energy -- halve it because pairwise
  vector<double> potentE = {potentialEnergy / 2};
  returnVector.push_back(potentE);

  return returnVector;
}

// Pairwise energy calculation for lj
double ljCalculator::getLennardJonesEnergy(double distance) {
  return 4 * epsilon * (pow(sigma / distance, 12) - pow(sigma / distance, 6));
}

// Pairwise force calculation for lj
double ljCalculator::getLennardJonesForce(double distance) {
  return -4 * epsilon *
         ((-12 * pow(sigma / distance, 13)) - (-6 * pow(sigma / distance, 7)));
}

////////////////////////////////// ase //////////////////////////////////////
aseCalculator::aseCalculator(std::string& cName, int* atomicNrs, const double* b) {
  atomicNumbers = atomicNrs;
  box = b;
  parseCalculatorSpec(cName, calculatorModule, calculatorClass, calculatorKwargs);
}

std::vector<std::vector<double>> aseCalculator::getFandU(std::vector<Atom*>& spheres) {
  return runAseCalculation(spheres, calculatorModule, calculatorClass, calculatorKwargs);
}
