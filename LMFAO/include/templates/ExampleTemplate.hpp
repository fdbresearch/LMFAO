//--------------------------------------------------------------------
//
// ExampleTemplate.hpp
//
// Created on: 12 Dec 2017
// Author: Max
//
//--------------------------------------------------------------------

#ifndef INCLUDE_TEMPLATE_EXAMPLE_HPP_
#define INCLUDE_TEMPLATE_EXAMPLE_HPP_

#include <iostream>
#include <vector>

#include <CppHelper.hpp>
#include <DataHandler.hpp>

const std::string PATH_TO_DATA = "data/example/";

namespace multifaq
{
    struct R_tuple
    {
        int a;
        int b;
        int c;

        R_tuple() {}
        
        R_tuple(std::vector<std::string>& fields) 
        {
            if (fields.size() < 3)
            {
                ERROR("R_tuple needs 3 input parameters!");
                exit(1);
            }
            a = std::stol(fields[0]);
            b = std::stod(fields[1]);
            c = std::stoi(fields[2]);
        }

        R_tuple(int a, int b, int c)
        {
            this->a = a;
            this->b = b;
            this->c = c;
        }
    };

    struct S_tuple
    {
        int a;
        int b;
        int d;

        S_tuple() {}

        S_tuple(std::vector<std::string>& fields) 
        {
            if (fields.size() != 3)
            {
                ERROR("S_tuple needs 3 input parameters!");
                exit(1);
            }
            a = std::stoi(fields[0]);
            b = std::stoi(fields[1]);
            d = std::stoi(fields[2]);
        }
        
        S_tuple(int a, int b, int d) 
        {
            this->a = a;
            this->b = b;
            this->d = d;
        }
    };
        
    struct T_tuple
    {
        int a;
        int e;

        T_tuple() {}

        T_tuple(std::vector<std::string>& fields) 
        {
            if (fields.size() != 2)
            {
                ERROR("T_tuple needs 2 input parameters!");
                exit(1);
            }
            a = std::stoi(fields[0]);
            e = std::stoi(fields[1]);
        }
    };
    
    struct U_tuple
    {
        int e;
        int f;
        
        U_tuple() {}
        
        U_tuple(std::vector<std::string>& fields) 
        {
            if (fields.size() != 2)
            {
                ERROR("U_tuple needs 2 input parameters!");
                exit(1);
            }
            e = std::stoi(fields[0]);
            f = std::stoi(fields[1]);
        }
    };

    /* TODO: can make relations have exact size */
    std::vector<R_tuple> R;
    std::vector<S_tuple> S;
    std::vector<T_tuple> T;
    std::vector<U_tuple> U;
    
    // std::vector<R_tuple>::iterator itR;
    // std::vector<S_tuple>::iterator itS;
    // std::vector<T_tuple>::iterator itT;
    // std::vector<U_tuple>::iterator itU;    
        
    //* TODO: Define all view-structs *//
    
    void load_relations()
    {
        R.clear();
        readFromFile(R, PATH_TO_DATA + "R.tbl", '|');

        S.clear();
        readFromFile(S, PATH_TO_DATA + "S.tbl", '|');

        T.clear();
        readFromFile(T, PATH_TO_DATA + "T.tbl", '|');

        U.clear();
        readFromFile(U, PATH_TO_DATA + "U.tbl", '|');
    }

    //* Construct the views - joining bottom up the tree and top down or
    //* something like this *//
    void computeViews()
    {
        load_relations();

        std::string result ="";

        // size_t l = 0, r = 4;
        // long lg = 3;
        // increment lower_pointer to at least upper bound
        // findUpperBound(R, &R_tuple::a, lg, l, r);
        // l = 0, r = 4;
        // findUpperBound(R, &R_tuple::b, 3.0,l,r);
        // l = 0, r = 4;
        // findUpperBound(R, &R_tuple::c, 3,l,r);
        // exit(0);
        
        // Sample join of R, S and T
        // std::string variableOrder[5] = {"A","B","C","D","E"};
        
        int max[2], min[2];
        size_t rel[2] = {};
        size_t lower_pointer_R[2] = {};
        size_t lower_pointer_S[2] = {};
        size_t lower_pointer_T[2] = {};
        size_t upper_pointer_R[2];        
        size_t upper_pointer_S[2];
        size_t upper_pointer_T[2];
        size_t numOfRel[2] = {3,2};
        bool found[2] = {}, atEnd[2] = {};
        
        upper_pointer_R[0] = R.size()-1;        
        upper_pointer_S[0] = S.size()-1;
        upper_pointer_T[0] = T.size()-1;
        
        /************************* ORDERING RELATIONS ***************/
        std::pair<int, int> order[3] =
            {
                std::make_pair(R[0].a, 1),
                std::make_pair(S[0].a, 2),
                std::make_pair(T[0].a, 3)
            };
        // TODO: here we add the relations that contain the variable and the
        // corrresponding ID from the treeDecomposition 
            
        std::sort(order, order + 3, sort_pred<int>());
        
        for (auto& p : order)
            std::cout << p.first << "  " << p.second << "\n";
        
        switch (order[0].second)
        {
        case 1: min[0] = R[lower_pointer_R[0]].a;
        case 2: min[0] = S[lower_pointer_S[0]].a;
        case 3: min[0] = T[lower_pointer_T[0]].a;
        }
        switch (order[2].second)
        {
        case 1: max[0] = R[lower_pointer_R[0]].a;
        case 2: max[0] = S[lower_pointer_S[0]].a;
        case 3: max[0] = T[lower_pointer_T[0]].a;
        }
        /************************* ORDERING RELATIONS ********************/

        rel[0] = 0;

        while (!atEnd[0])  
        {
            found[0] = false;
            
            //*********** Seek Value 
            do 
            {
                std::cout << "relation: " << order[rel[0]].second << std::endl;
                
                switch (order[rel[0]].second)
                {
                case 1: // TODO: these integers should be the same as the ids in TD
                    if (R[lower_pointer_R[0]].a == max[0])
                    {
                        std::cout << "found value in R \n";
                        found[0] = true;
                    }
                    else if (max[0] > R[R.size()-1].a)
                    {
                        std::cout << "max is bigger than R (at END) \n";
                        atEnd[0] = true;
                        break;
                    }
                    else
                    {
                        std::cout << "findUpperBound for R \n";
                        // increment lower_pointer to at least upper bound
                        findUpperBound(R, &R_tuple::a, max[0],
                                       lower_pointer_R[0], R.size()-1);
                        max[0] = R[lower_pointer_R[0]].a;
                        rel[0] = (rel[0] + 1) % numOfRel[0];
                    }
                    break;
                case 2:
                    if (S[lower_pointer_S[0]].a == max[0])
                    {
                        std::cout << "found value in S \n";
                        found[0] = true;
                    }
                    else if (max[0] > S[S.size()-1].a)
                    {
                        std::cout << "max is bigger than S (at END) \n";
                        atEnd[0] = true;
                        break;
                    }
                    else
                    {
                         std::cout << "findUpperBound for S \n";
                         std::cout << "S < max "<<max[1]<<"\n";
                        // increment lower_pointer to at least upper bound
                        findUpperBound(S, &S_tuple::a,max[0],
                                       lower_pointer_S[0], S.size()-1);
                        max[0] = S[lower_pointer_S[0]].a;
                        rel[0] = (rel[0] + 1) % numOfRel[0];
                    }
                    break;
                case 3:
                    if (T[lower_pointer_T[0]].a == max[0])
                    {
                        std::cout << "found value in T \n";
                        
                        found[0] = true;
                    }
                    else if (max[0] > T[T.size()-1].a)
                    {
                        std::cout << "T is atEnd() \n";
                        
                        atEnd[0] = true;
                        break;
                    }
                    else
                    {
                        std::cout << "T find nextUpperBound \n";
                        
                        // increment lower_pointer to at least upper bound
                        findUpperBound(T, &T_tuple::a, max[0],
                                       lower_pointer_T[0], T.size()-1);
                        
                        max[0] = T[lower_pointer_T[0]].a;
                        rel[0] = (rel[0] + 1) % numOfRel[0];
                    }
                    break;
                }
            } while (!found[0] && !atEnd[0]);
            //*********** End Seek Value  ******************//

            /* If at end - break loop */
            if (atEnd[0])
                break;
            
            std::cout << "new A value: "<< max[0] << std::endl;
            upper_pointer_R[0] = lower_pointer_R[0];
            upper_pointer_S[0] = lower_pointer_S[0];
            upper_pointer_T[0] = lower_pointer_T[0];

            /* Update R range */ 
            while (upper_pointer_R[0]<R.size()-1 && R[upper_pointer_R[0]+1].a == max[0])
            {
                std::cout << R[upper_pointer_R[0]].a << "\n";
                ++upper_pointer_R[0];
            }

            /* Update S range */
            while (upper_pointer_S[0]<S.size()-1 && S[upper_pointer_S[0]+1].a == max[0])
            {
                ++upper_pointer_S[0];
            }
            
            /* Update T range */
            while (upper_pointer_T[0]<T.size()-1 && T[upper_pointer_T[0]+1].a == max[0])
            {
                ++upper_pointer_T[0];
            }

            std::cout << "-------------- Range A after setting upper_pointer \n";
            std::cout << lower_pointer_R[0] << "  " <<  upper_pointer_R[0] << "\n";
            std::cout << lower_pointer_S[0] << "  " <<  upper_pointer_S[0] << "\n";
            std::cout << lower_pointer_T[0] << "  " <<  upper_pointer_T[0] << "\n";
            std::cout << "--------------\n";

            /************ SET RANGES FOR NEXT VAR  ***************/

            upper_pointer_R[1] = upper_pointer_R[0];
            upper_pointer_S[1] = upper_pointer_S[0];
            upper_pointer_T[1] = upper_pointer_T[0];

            lower_pointer_R[1] = lower_pointer_R[0];
            lower_pointer_S[1] = lower_pointer_S[0];        
            lower_pointer_T[1] = lower_pointer_T[0];

            /************ SET RANGES FOR NEXT VAR  ***************/
            
            /************************* ORDERING RELATIONS ***************/
            // TODO:FIXME:if there are less than three relations avoid the
            // vector and ordering -- do add them order right away!  
            std::pair<int, int> order[2] =
                {
                    std::make_pair(R[lower_pointer_R[1]].b, 1), 
                    std::make_pair(S[lower_pointer_S[1]].b, 2)
                };
            // TODO: here we add the relations that contain the variable and the
            // corrresponding ID from the treeDecomposition 
            
            std::sort(order, order + 2, sort_pred<int>());
            
            switch (order[0].second)
            {
            case 1: min[1] = R[lower_pointer_R[1]].b;
            case 2: min[1] = S[lower_pointer_S[1]].b;
            }
            switch (order[1].second)
            {
            case 1: max[1] = R[lower_pointer_R[1]].b;
            case 2: max[1] = S[lower_pointer_S[1]].b;
            }
            /************************* ORDERING RELATIONS ********************/

            rel[1] = 0;
            atEnd[1] = false;

            // move on to variable 1 - (depth 1)
            while (!atEnd[1])
            {
                found[1] = false;
            
                //*********** Seek Value ***********//
                do 
                {
                    std::cout << "here "<< order[rel[1]].second <<"\n";
                    std::cout <<  order[rel[1]].second <<"\n";
                    
                    switch (order[rel[1]].second)
                    {
                    case 1: // TODO: these integers should be the same as the ids in TD
                        if (R[lower_pointer_R[1]].b == max[1])
                        {
                            std::cout << "found value in R \n";
                            found[1] = true;
                        }
                        else if (max[1] > R[upper_pointer_R[0]].b)
                        {
                            std::cout << "max is bigger than R (at END) \n";
                            atEnd[1] = true;
                            break;
                        }
                        else
                        {
                            std::cout << "findUpperBound for R \n";
                            // increment lower_pointer to at least upper bound
                            findUpperBound(R, &R_tuple::b, max[1],
                                           lower_pointer_R[1], upper_pointer_R[0]);
                            max[1] = R[lower_pointer_R[1]].b;
                            rel[1] = (rel[1] + 1) % numOfRel[1];
                        }
                        break;
                    case 2:
                        if (S[lower_pointer_S[1]].b == max[1])
                        {
                            std::cout << "found value in S \n";
                            found[1] = true;
                        }
                        else if (max[1] > S[upper_pointer_S[0]].b)
                        {
                            std::cout << "max is bigger than S (at END) \n";
                            std::cout << max[1] << " " <<
                                S[lower_pointer_S[1]].b<<std::endl;
                            atEnd[1] = true;
                        }
                        else
                        {
                            std::cout << "findUpperBound for S \n";
                            std::cout << "S < max "<<max[1]<<"\n";
                            
                            // increment lower_pointer to at least upper bound
                            findUpperBound(S, &S_tuple::b ,max[1],
                                           lower_pointer_S[1], upper_pointer_S[0]);

                            std::cout << "lower_pointer: " << lower_pointer_S[1]
                                      << " " <<S[lower_pointer_S[1]].b << std::endl;
                            
                            max[1] = S[lower_pointer_S[1]].b;
                            rel[1] = (rel[1] + 1) % numOfRel[1];
                        }
                        break;
                    }
                } while (!found[1] && !atEnd[1]);
                //*********** End Seek Value

                /* If at end - break loop */
                if (atEnd[1])
                    break;

                std::cout << "new B value: "<< max[1] << std::endl;
                
                upper_pointer_R[1] = lower_pointer_R[1];
                upper_pointer_S[1] = lower_pointer_S[1];

                /* Update R range */ 
                while (upper_pointer_R[1] < upper_pointer_R[0] &&
                       R[upper_pointer_R[1] + 1].b == max[1])
                {
                    ++upper_pointer_R[1];
                }

                /* Update S range */
                while (upper_pointer_S[1] < upper_pointer_S[0] &&
                       S[upper_pointer_S[1] + 1].b == max[1])
                {
                    ++upper_pointer_S[1];
                }
                
                std::cout << "-------------- Range B after updating ranges \n";
                std::cout << lower_pointer_R[1] << "  " <<  upper_pointer_R[1] << "\n";
                std::cout << lower_pointer_S[1] << "  " <<  upper_pointer_S[1] << "\n";
                std::cout << lower_pointer_T[1] << "  " <<  upper_pointer_T[1] << "\n";
                std::cout << "--------------\n";

                // DO CARTESIAN PRODUCT!!
                std::cout << "cartesian product \n";
                
                for (size_t r_idx = lower_pointer_R[1]; r_idx <= upper_pointer_R[1];
                     ++r_idx)
                {
                    for (size_t s_idx = lower_pointer_S[1]; s_idx <= upper_pointer_S[1];
                         ++s_idx)
                    {
                        for (size_t t_idx = lower_pointer_T[1]; t_idx <=
                                 upper_pointer_T[1]; ++t_idx)
                        {
                            std::cout << R[r_idx].a << ", " <<  R[r_idx].b << ", "
                                      <<  R[r_idx].c << ", " << S[s_idx].d << ", "
                                      <<  T[t_idx].e << "\n";
                            result += std::to_string(R[r_idx].a)+","+
                                std::to_string(R[r_idx].b)+","+
                                std::to_string(R[r_idx].c)+","+
                                std::to_string(S[s_idx].d)+","+
                                std::to_string(T[t_idx].e)+"\n";
                        }
                    }
                }

                lower_pointer_R[1] = upper_pointer_R[1];
                lower_pointer_S[1] = upper_pointer_S[1];
                lower_pointer_T[1] = upper_pointer_T[1];
                std::cout << "rel: " << order[rel[1]].second << std::endl;
                
                switch (order[rel[1]].second)
                {
                case 1:
                    // TODO: these integers should be the same as the ids in TD
                    lower_pointer_R[1] += 1;
                    if (lower_pointer_R[1] > upper_pointer_R[0])
                        atEnd[1] = true;
                    else
                    {
                        max[1] = R[lower_pointer_R[1]].b;
                        rel[1] = (rel[1] + 1) % numOfRel[1];
                    }
                    break;
                case 2:
                    lower_pointer_S[1] += 1;
                    if (lower_pointer_S[1] > upper_pointer_S[0])
			atEnd[1] = true;
                    else
                    {
                        max[1] = S[lower_pointer_S[1]].b;
                        rel[1] = (rel[1] + 1) % numOfRel[1];
                    }
                    break;
                }

                std::cout << "rel: " << order[rel[1]].second << std::endl;

                std::cout << "-------------- Range B after resetting pointers \n";
                std::cout << lower_pointer_R[1] << "  " <<  upper_pointer_R[1] << "\n";
                std::cout << lower_pointer_S[1] << "  " <<  upper_pointer_S[1] << "\n";
                std::cout << lower_pointer_T[1] << "  " <<  upper_pointer_T[1] << "\n";
                std::cout << "--------------\n";
            }

            lower_pointer_R[0] = upper_pointer_R[0];
            lower_pointer_S[0] = upper_pointer_S[0];
            lower_pointer_T[0] = upper_pointer_T[0];

            switch (order[rel[0]].second)
            {
            case 1: // TODO: these integers should be the same as the ids in TD
                lower_pointer_R[0] += 1;
                if (lower_pointer_R[0] > R.size()-1)
                {
                    std::cout << "R is at end \n";
                    atEnd[0] = true;
                }
                else
                {
                    max[0] = R[lower_pointer_R[0]].a;
                    rel[0] = (rel[0] + 1) % numOfRel[0];
                }
                
                break;
            case 2:
                lower_pointer_S[0] += 1;
                if (lower_pointer_S[0] > S.size()-1)
                {
                    std::cout << "S is at end \n";
                    atEnd[0] = true;
                }
                else
                {
                    max[0] = S[lower_pointer_S[0]].a;
                    rel[0] = (rel[0] + 1) % numOfRel[0];
                }
                break;
            case 3:
                lower_pointer_T[0] += 1;
                if (lower_pointer_T[0] > T.size()-1)
                {
                    std::cout << "T is at end \n";
                    atEnd[0] = true;
                }
                else
                {
                    max[0] = T[lower_pointer_T[0]].a;
                    rel[0] = (rel[0] + 1) % numOfRel[0];
                }
                break;
            }

            std::cout << "-------------- Range A after resetting pointers \n";
            std::cout << lower_pointer_R[0] << "  " <<  upper_pointer_R[0] << "\n";
            std::cout << lower_pointer_S[0] << "  " <<  upper_pointer_S[0] << "\n";
            std::cout << lower_pointer_T[0] << "  " <<  upper_pointer_T[0] << "\n";
            std::cout << "--------------\n";
            std::cout << "new max: " << max[0] << "\n";
            std::cout << "atEnd(1) " << atEnd[0]<< "\n";
            
        }
            std::cout << result << "\n";
    }
}

#endif /* INCLUDE_TEMPLATE_EXAMPLE_HPP_ */
