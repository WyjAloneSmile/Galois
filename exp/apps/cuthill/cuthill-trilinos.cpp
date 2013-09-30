/**
 * Use RCM ordering from Trilinos library.
 */
#include "Ifpack_Reordering.h"
#include "Ifpack_RCMReordering.h"
#include "Ifpack_Graph.h"
#include "llvm/Support/CommandLine.h"
#include "Epetra_CrsMatrix.h"
#include "Epetra_SerialComm.h"
#include "Galois/Graph/FileGraph.h"
#include "Galois/Statistic.h"

#include <iostream>

namespace cll = llvm::cl;
static cll::opt<std::string> filename(cll::Positional, cll::desc("<input file>"), cll::Required);
static cll::opt<int> startNode("startnode", cll::desc("Node to start reordering from."), cll::init(0));

int main(int argc, char** argv) {
  Galois::StatManager statManager;
  Epetra_SerialComm comm;
  Galois::Graph::FileGraph fileGraph;

  Galois::StatTimer Ttotal("TotalTime");
  Ttotal.start();

  llvm::cl::ParseCommandLineOptions(argc, argv);
  fileGraph.structureFromFileInterleaved<void>(filename);

  // Load matrix from graph
  Epetra_Map rowMap(static_cast<int>(fileGraph.size()), 0, comm);
  std::vector<int> globalElements(rowMap.NumMyElements());
  rowMap.MyGlobalElements(&globalElements[0]);
  std::vector<int> nonzeros(rowMap.NumMyElements());
  for (size_t i = 0; i < nonzeros.size(); ++i) {
    nonzeros[i] = std::distance(fileGraph.edge_begin(globalElements[i]), fileGraph.edge_end(globalElements[i]));
  }
  Epetra_CrsMatrix A(Epetra_DataAccess::Copy, rowMap, &nonzeros[0]);
  for (size_t i = 0; i < nonzeros.size(); ++i) {
    double value = 1.0;
    int src = globalElements[i];
    for (Galois::Graph::FileGraph::edge_iterator ii = fileGraph.edge_begin(src), ei = fileGraph.edge_end(src); ii != ei; ++ii) {
      int dst = fileGraph.getEdgeDst(ii);
      A.InsertGlobalValues(globalElements[i], 1, &value, &dst);
    }
  }
  A.FillComplete();

  // Reorder matrix
  Galois::StatTimer T;
  T.start();
  Ifpack_RCMReordering reordering;
  reordering.SetParameter("reorder: root node", startNode);
  Galois::StatTimer Tcuthill("CuthillTime");
  Tcuthill.start();
  IFPACK_CHK_ERR(reordering.Compute(A));
  Tcuthill.stop();
  T.stop();

  Ttotal.stop();

  //std::cout << reordering;

  return 0;
}
