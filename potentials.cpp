#include "potentials.h"

#include <Python.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "atom.h"

extern double centerCoords[3];

namespace {

constexpr double kDistanceScale = 0.02;

[[noreturn]] void failWithPythonError(const char* message) {
  PyErr_Print();
  std::cerr << message << std::endl;
  std::exit(1);
}

void ensurePythonInitialized() {
  if (!Py_IsInitialized()) {
    Py_Initialize();
  }
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

  if (spec == "lj" || spec == "lennard-jones") {
    moduleName = "ase.calculators.lj";
    className = "LennardJones";
    kwargsText.clear();
    return;
  }

  if (spec == "morse") {
    moduleName = "ase.calculators.morse";
    className = "MorsePotential";
    kwargsText.clear();
    return;
  }

  if (spec == "emt") {
    moduleName = "ase.calculators.emt";
    className = "EMT";
    kwargsText.clear();
    return;
  }

  size_t firstColon = spec.find(':');
  if (firstColon == std::string::npos) {
    std::cerr << "ASE calculator spec must be empty, a known alias "
                 "(`lj`, `morse`, `emt`), or `module:Class[:kwargs]`."
              << std::endl;
    std::exit(1);
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

PyObject* callMethodNoArgs(PyObject* object, const char* methodName, const char* errorMessage) {
  PyObject* result = PyObject_CallMethod(object, methodName, nullptr);
  if (result == nullptr) {
    failWithPythonError(errorMessage);
  }
  return result;
}

PyObject* sequenceFast(PyObject* object, const char* errorMessage) {
  PyObject* sequence = PySequence_Fast(object, errorMessage);
  if (sequence == nullptr) {
    failWithPythonError(errorMessage);
  }
  return sequence;
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

PyObject* buildCellList(const std::array<double, 9>& cellMatrix) {
  PyObject* cell = PyList_New(3);
  if (cell == nullptr) {
    failWithPythonError("Failed to allocate Python list for cell.");
  }

  for (Py_ssize_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
    PyObject* row = PyList_New(3);
    if (row == nullptr) {
      Py_DECREF(cell);
      failWithPythonError("Failed to allocate Python cell row.");
    }

    for (Py_ssize_t columnIndex = 0; columnIndex < 3; ++columnIndex) {
      const size_t flatIndex = static_cast<size_t>(rowIndex * 3 + columnIndex);
      PyObject* value = PyFloat_FromDouble(cellMatrix[flatIndex]);
      if (value == nullptr) {
        Py_DECREF(row);
        Py_DECREF(cell);
        failWithPythonError("Failed to convert cell value for Python.");
      }
      PyList_SetItem(row, columnIndex, value);
    }

    PyList_SetItem(cell, rowIndex, row);
  }

  return cell;
}

PyObject* buildPbcList(const std::array<int, 3>& periodicBoundaryConditions) {
  PyObject* pbc = PyList_New(3);
  if (pbc == nullptr) {
    failWithPythonError("Failed to allocate Python list for PBC.");
  }

  for (Py_ssize_t index = 0; index < 3; ++index) {
    PyObject* value = PyBool_FromLong(periodicBoundaryConditions[static_cast<size_t>(index)]);
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
                                                   const std::string& kwargsText,
                                                   const std::array<double, 9>& cellMatrix,
                                                   const std::array<int, 3>& periodicBoundaryConditions) {
  ensurePythonInitialized();

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
  PyObject* cellObject = buildCellList(cellMatrix);
  PyObject* pbcObject = buildPbcList(periodicBoundaryConditions);
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

std::array<double, 9> extractCellMatrix(PyObject* cellObject) {
  std::array<double, 9> cell = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
  PyObject* rows = sequenceFast(cellObject, "ASE cell must be a 3x3 sequence.");
  if (PySequence_Fast_GET_SIZE(rows) != 3) {
    Py_DECREF(rows);
    std::cerr << "ASE cell must contain exactly three cell vectors." << std::endl;
    std::exit(1);
  }

  for (Py_ssize_t rowIndex = 0; rowIndex < 3; ++rowIndex) {
    PyObject* row = sequenceFast(PySequence_Fast_GET_ITEM(rows, rowIndex),
                                 "ASE cell row must be a sequence of length 3.");
    if (PySequence_Fast_GET_SIZE(row) != 3) {
      Py_DECREF(row);
      Py_DECREF(rows);
      std::cerr << "Each ASE cell vector must have exactly three coordinates." << std::endl;
      std::exit(1);
    }

    for (Py_ssize_t columnIndex = 0; columnIndex < 3; ++columnIndex) {
      cell[static_cast<size_t>(rowIndex * 3 + columnIndex)] =
          PyFloat_AsDouble(PySequence_Fast_GET_ITEM(row, columnIndex));
      if (PyErr_Occurred()) {
        Py_DECREF(row);
        Py_DECREF(rows);
        failWithPythonError("Failed to convert ASE cell value to C++.");
      }
    }
    Py_DECREF(row);
  }

  Py_DECREF(rows);
  return cell;
}

std::array<int, 3> extractPbc(PyObject* pbcObject) {
  std::array<int, 3> pbc = {0, 0, 0};
  PyObject* items = sequenceFast(pbcObject, "ASE pbc must be a sequence of length 3.");
  if (PySequence_Fast_GET_SIZE(items) != 3) {
    Py_DECREF(items);
    std::cerr << "ASE pbc must contain exactly three booleans." << std::endl;
    std::exit(1);
  }

  for (Py_ssize_t index = 0; index < 3; ++index) {
    pbc[static_cast<size_t>(index)] = PyObject_IsTrue(PySequence_Fast_GET_ITEM(items, index));
  }

  Py_DECREF(items);
  return pbc;
}

std::vector<int> extractAtomicNumbers(PyObject* numbersObject) {
  std::vector<int> atomicNumbers;
  PyObject* items = sequenceFast(numbersObject, "ASE atomic numbers must be a sequence.");
  atomicNumbers.reserve(static_cast<size_t>(PySequence_Fast_GET_SIZE(items)));

  for (Py_ssize_t index = 0; index < PySequence_Fast_GET_SIZE(items); ++index) {
    atomicNumbers.push_back(static_cast<int>(PyLong_AsLong(PySequence_Fast_GET_ITEM(items, index))));
    if (PyErr_Occurred()) {
      Py_DECREF(items);
      failWithPythonError("Failed to convert ASE atomic number to C++.");
    }
  }

  Py_DECREF(items);
  return atomicNumbers;
}

std::vector<std::array<double, 3>> extractPositions(PyObject* positionsObject) {
  std::vector<std::array<double, 3>> positions;
  PyObject* rows = sequenceFast(positionsObject, "ASE positions must be an Nx3 sequence.");
  positions.reserve(static_cast<size_t>(PySequence_Fast_GET_SIZE(rows)));

  for (Py_ssize_t rowIndex = 0; rowIndex < PySequence_Fast_GET_SIZE(rows); ++rowIndex) {
    PyObject* row = sequenceFast(PySequence_Fast_GET_ITEM(rows, rowIndex),
                                 "ASE position row must be a sequence of length 3.");
    if (PySequence_Fast_GET_SIZE(row) != 3) {
      Py_DECREF(row);
      Py_DECREF(rows);
      std::cerr << "Each ASE position row must have exactly three coordinates." << std::endl;
      std::exit(1);
    }

    std::array<double, 3> position = {
        PyFloat_AsDouble(PySequence_Fast_GET_ITEM(row, 0)),
        PyFloat_AsDouble(PySequence_Fast_GET_ITEM(row, 1)),
        PyFloat_AsDouble(PySequence_Fast_GET_ITEM(row, 2))};
    if (PyErr_Occurred()) {
      Py_DECREF(row);
      Py_DECREF(rows);
      failWithPythonError("Failed to convert ASE position to C++.");
    }
    positions.push_back(position);
    Py_DECREF(row);
  }

  Py_DECREF(rows);
  return positions;
}

}  // namespace

AseStructureData loadAseStructure(const std::string& filename) {
  ensurePythonInitialized();
  PyGILState_STATE gilState = PyGILState_Ensure();

  PyObject* ioModule = importModule("ase.io");
  PyObject* readFunction = getCallable(ioModule, "read");
  PyObject* filenameObject = PyUnicode_FromString(filename.c_str());
  if (filenameObject == nullptr) {
    Py_DECREF(readFunction);
    Py_DECREF(ioModule);
    failWithPythonError("Failed to convert input filename for ASE.");
  }

  PyObject* atomsObject = PyObject_CallFunctionObjArgs(readFunction, filenameObject, nullptr);
  Py_DECREF(filenameObject);
  Py_DECREF(readFunction);
  Py_DECREF(ioModule);
  if (atomsObject == nullptr) {
    failWithPythonError("ASE failed to read the structure file.");
  }

  PyObject* positionsObject =
      callMethodNoArgs(atomsObject, "get_positions", "ASE failed to extract positions from the file.");
  PyObject* numbersObject = callMethodNoArgs(
      atomsObject, "get_atomic_numbers", "ASE failed to extract atomic numbers from the file.");
  PyObject* cellObject =
      callMethodNoArgs(atomsObject, "get_cell", "ASE failed to extract the cell from the file.");
  PyObject* pbcObject =
      callMethodNoArgs(atomsObject, "get_pbc", "ASE failed to extract PBC from the file.");

  AseStructureData structure;
  structure.positions = extractPositions(positionsObject);
  structure.atomicNumbers = extractAtomicNumbers(numbersObject);
  structure.cell = extractCellMatrix(cellObject);
  structure.pbc = extractPbc(pbcObject);

  Py_DECREF(positionsObject);
  Py_DECREF(numbersObject);
  Py_DECREF(cellObject);
  Py_DECREF(pbcObject);
  Py_DECREF(atomsObject);

  if (structure.positions.size() != structure.atomicNumbers.size()) {
    PyGILState_Release(gilState);
    throw std::runtime_error("ASE returned mismatched positions and atomic numbers.");
  }

  PyGILState_Release(gilState);
  return structure;
}

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
aseCalculator::aseCalculator(const std::string& cName,
                             const std::array<double, 9>& cellMatrix,
                             const std::array<int, 3>& periodicBoundaryConditions)
    : cell(cellMatrix), pbc(periodicBoundaryConditions) {
  parseCalculatorSpec(cName, calculatorModule, calculatorClass, calculatorKwargs);
}

std::vector<std::vector<double>> aseCalculator::getFandU(std::vector<Atom*>& spheres) {
  return runAseCalculation(spheres, calculatorModule, calculatorClass, calculatorKwargs, cell, pbc);
}
