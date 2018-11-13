#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include <vector>
#include <distributions.h>

template <typename T>
class BasisExpansion
{
  public:
    static vector<vector<T> > expand(T x, int degree);
};

template <typename T>
vector<vector<T> > BasisExpansion<T>::expand(T x, int degree) {
    assert (degree >= 0);

    vector<vector<T> > answer;
    answer.resize(1);
    T current = 1;
    for (int i = 0; i <= degree; ++i) {
        answer[0].push_back(current);
        current *= x;
    }
    return answer;
}

template <typename T>
class Polynomial
{
  private:
    std::vector<T> d_coeff;
    Gaussian<T> *gauss;
  public:
    Polynomial(std::vector<T> coeff, Gaussian<T> *G = nullptr);
        /// {a0, a1, ... an-1} : a0*x^0 + a1*x^1 + ... +a^n-1 * x^N-1

    T evaluate(T x);
};

template <typename T>
Polynomial<T>::Polynomial(std::vector<T> coeff, Gaussian<T> *G)
{
    d_coeff = coeff;
    gauss = G;
}

template <typename T>
T Polynomial<T>::evaluate(T x)
{
    T result = 0;
    for (int it = (int)d_coeff.size() - 1; it >= 0; --it) {
        result = result * x;
        result += d_coeff[it];
    }
    if (gauss != nullptr) {
        result += gauss->sample();
    }
    return result;
}

#endif // POLYNOMIAL_H
