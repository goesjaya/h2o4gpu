#include <limits>
#include <random>
#include <vector>

#include "mat_gen.h"
#include "pogs.h"
#include "timer.h"

using namespace pogs;

template <typename T>
T MaxDiff(std::vector<T> *v1, std::vector<T> *v2) {
  T max_diff = 0;
#pragma omp parallel for reduction(max : max_diff)
  for (unsigned int i = 0; i < v1->size(); ++i)
    max_diff = std::max(max_diff, std::abs((*v1)[i] - (*v2)[i]));
  return max_diff;
}

template <typename T>
T Asum(std::vector<T> *v) {
  T asum = 0;
#pragma omp parallel for reduction(+ : asum)
  for (unsigned int i = 0; i < v->size(); ++i)
    asum += std::abs((*v)[i]);
  return asum;
}


// LassoPath
//   minimize    (1/2) ||Ax - b||_2^2 + \lambda ||x||_1
//
// for 50 values of \lambda.
// See <pogs>/matlab/examples/lasso_path.m for detailed description.
template <typename T>
double LassoPath(int m, int n, int nnz) {
  unsigned int nlambda = 100;
  std::vector<T> val(nnz);
  std::vector<int> col_ind(nnz);
  std::vector<int> row_ptr(m + 1);
  std::vector<T> b(m);
  std::vector<T> x(n);
  std::vector<T> x_last(n, std::numeric_limits<T>::max());
  std::vector<T> y(m);

  std::default_random_engine generator;
  std::normal_distribution<T> n_dist(static_cast<T>(0),
                                     static_cast<T>(1));
 
  std::vector<std::tuple<int, int, T>> entries;
  nnz = MatGenApprox(m, n, nnz, val.data(), row_ptr.data(), col_ind.data(),
      static_cast<T>(-1), static_cast<T>(1), entries);

  for (unsigned int i = 0; i < m; ++i)
    b[i] = static_cast<T>(4) * n_dist(generator);

  std::vector<T> u(n, static_cast<T>(0));
  for (unsigned int i = 0; i < m; ++i) {
    for (unsigned int j = row_ptr[i]; j < row_ptr[i + 1]; ++j) {
      u[col_ind[j]] += val[j] * b[i];
    }
  }

  T lambda_max = 0;
  for (unsigned int i = 0; i < n; ++i)
    lambda_max = std::max(lambda_max, std::abs(u[i]));

  Sparse<T, int, ROW> A_(val.data(), row_ptr.data(), col_ind.data(), nnz);
  PogsData<T, Sparse<T, int, ROW>> pogs_data(A_, m, n);
  pogs_data.x = x.data();
  pogs_data.y = y.data();

  pogs_data.f.reserve(m);
  for (unsigned int i = 0; i < m; ++i)
    pogs_data.f.emplace_back(kSquare, static_cast<T>(1), b[i]);

  pogs_data.g.reserve(n);
  for (unsigned int i = 0; i < n; ++i)
    pogs_data.g.emplace_back(kAbs);

  AllocSparseFactors(&pogs_data);

  double t = timer<double>();
  for (unsigned int i = 0; i < nlambda; ++i) {
    T lambda = std::exp((std::log(lambda_max) * (nlambda - 1 - i) +
        static_cast<T>(1e-2) * std::log(lambda_max) * i) / (nlambda - 1));
    printf("max = %e\n", lambda);

    for (unsigned int i = 0; i < n; ++i)
      pogs_data.g[i].c = lambda;

    Solve(&pogs_data);
    
    if (MaxDiff(&x, &x_last) < 1e-3 * Asum(&x))
      break;
    x_last = x;
  }
  FreeSparseFactors(&pogs_data);

  return timer<double>() - t;
}

template double LassoPath<double>(int m, int n, int nnz);
template double LassoPath<float>(int m, int n, int nnz);

