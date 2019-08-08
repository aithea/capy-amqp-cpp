//
// Created by denn nevera on 2019-05-31.
//

#include "capy/amqp.h"
#include "gtest/gtest.h"

#include <algorithm>
#include <regex>
#include <string>
#include <vector>

std::vector<float > split_by_delimiter(const std::string& s, char delimiter)
{
  std::string token;
  std::istringstream stream(s);
  std::vector<float> tokens;

  while (std::getline(stream, token, delimiter))
    tokens.push_back(std::stof(token));

  return tokens;
}

std::vector<float> split_by_pattern(const std::string& content, const std::string& pattern){

  auto p = std::regex(pattern);
  std::vector<float> floats;

  std::transform(
          std::sregex_token_iterator(content.begin(), content.end(), p, -1),
          std::sregex_token_iterator(), back_inserter(floats),
          [](const std::string& name){
              return std::stof(name);
          });

  return floats;
}


TEST(Initial, InitialTest) {

  /***
   * a simple space separated string to floats
   */
  std::string float_list = "1.02 122.12  12.4 12.42";

  auto iss = std::istringstream(float_list);

  std::vector<float> v;

  std::copy(std::istream_iterator<float>(iss), std::istream_iterator<float>(), std::back_inserter(v));

  std::cout << std::endl;

  std::copy(v.begin(), v.end(), std::ostream_iterator<float>(std::cout, " \n"));

  std::cout << std::endl;

  /***
   * float digits separated any delemiters
   */
  float_list = "1.02, 122.12 ,  12.4,12.42";

  auto floats = split_by_pattern(float_list, "\\s*,\\s*");

  std::copy(floats.begin(), floats.end(), std::ostream_iterator<float>(std::cout, " \n"));

  std::cout << std::endl;

  /***
   * float digits separated regular char delimiter
   */
  float_list = "1.02, 122.12, 12.4, 12.42";

  floats = split_by_delimiter(float_list, ',');

  std::copy(floats.begin(), floats.end(), std::ostream_iterator<float>(std::cout, " \n"));

  std::cout << std::endl;

}