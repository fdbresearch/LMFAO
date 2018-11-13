#ifndef DISTRIBUTIONS_H
#define DISTRIBUTIONS_H

#include <random>
#include <chrono>
#include <iostream>


template <typename T>
class Distributions
{
  public:
    static std::pair<std::default_random_engine, std::normal_distribution<T> > normalDistribution(T mean, T standardDeviation);
};

template <typename T>
std::pair<std::default_random_engine, std::normal_distribution<T> > Distributions<T>::normalDistribution(T mean, T standardDeviation)
{
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::normal_distribution<T> normal(mean, standardDeviation);
    return {generator, normal};
}


template <typename T>
class Gaussian
{
  private:
    std::pair<std::default_random_engine, std::normal_distribution<T> > d_data;
  public:
    Gaussian(T meanValue, T standardDeviationValue);
    T sample();
    T mean();
    T standardDeviation();
};

template <typename T>
Gaussian<T>::Gaussian(T meanValue, T standardDeviationValue)
{
    d_data = Distributions<T>::normalDistribution(meanValue, standardDeviationValue);
}

template <typename T>
T Gaussian<T>::sample()
{
    return d_data.second(d_data.first);
}

template <typename T>
T Gaussian<T>::mean()
{
    return d_data.second.mean();
}

template <typename T>
T Gaussian<T>::standardDeviation()
{
    return d_data.second.stddev();
}


#endif // DISTRIBUTIONS_H
