# TODO:
1. ~~Add null node for end of the regex~~
1. ~~Implement DFS/BFS for matching the strings with the regexes~~

# Benchmarking:
Pre-computing 0.7459ms (my)

Matching '' resulted in:

                 Success 0.0325ms (my) vs 0.3177ms (O(n^2))

Matching 'a' resulted in:

                 Success 0.0315ms (my) vs 0.2826ms (O(n^2))

Matching 'aa' resulted in:

                 Success 0.1174ms (my) vs 0.2829ms (O(n^2))

Matching 'aaa' resulted in:

                 Success 0.0527ms (my) vs 0.278ms (O(n^2))

Matching 'aaaa' resulted in:

                 Success 0.0573ms (my) vs 0.2851ms (O(n^2))

Matching '' resulted in:

                 Success 0.01ms (my) vs 0.2462ms (O(n^2))

Matching 'a' resulted in:

                 Success 0.041ms (my) vs 0.2812ms (O(n^2))

Matching 'b' resulted in:

                 Success 0.0263ms (my) vs 0.2812ms (O(n^2))

Matching 'c' resulted in:

                 Success 0.0258ms (my) vs 0.2804ms (O(n^2))

Matching 'd' resulted in:

                 Success 0.0169ms (my) vs 0.2922ms (O(n^2))

Matching 'df' resulted in:

                 Success 0.0481ms (my) vs 0.3089ms (O(n^2))

Matching 'e' resulted in:

                 Success 0.0189ms (my) vs 0.3446ms (O(n^2))

Matching 'ef' resulted in:

                 Success 0.0266ms (my) vs 0.2766ms (O(n^2))

Matching 'fdg' resulted in:

                 Success 0.046ms (my) vs 0.2965ms (O(n^2))

Matching 'fdgg' resulted in:

                 Success 0.064ms (my) vs 0.3206ms (O(n^2))

Matching 'fd' resulted in:

                 Success 0.0208ms (my) vs 0.286ms (O(n^2))

Matching 'fdabgg' resulted in:

                 Success 0.0624ms (my) vs 0.2931ms (O(n^2))

Matching 'fdababgg' resulted in:

                 Success 0.0695ms (my) vs 0.302ms (O(n^2))

Matching 'fdabcdgg' resulted in:

                 Success 0.5614ms (my) vs 0.3895ms (O(n^2))

Matching 'fdcdcdgg' resulted in:

                 Success 0.067ms (my) vs 0.3062ms (O(n^2))

Matching 'fdacgg' resulted in:

                 Success 0.0247ms (my) vs 0.2989ms (O(n^2))

Matching 'fadabgg' resulted in:

                 Success 0.0576ms (my) vs 0.2872ms (O(n^2))

Matching 'fadababgg' resulted in:

                 Success 0.0665ms (my) vs 0.2943ms (O(n^2))

Matching 'fadabcdgg' resulted in:

                 Success 0.0908ms (my) vs 0.3553ms (O(n^2))

Matching 'fadcdcdgg' resulted in:

                 Success 0.0963ms (my) vs 0.323ms (O(n^2))

Matching 'fadacgg' resulted in:

                 Success 0.0408ms (my) vs 0.2973ms (O(n^2))

Matching 'fbdabgg' resulted in:

                 Success 0.0584ms (my) vs 0.292ms (O(n^2))

Matching 'fbdababgg' resulted in:

                 Success 0.0679ms (my) vs 0.2974ms (O(n^2))

Matching 'fbdabcdgg' resulted in:

                 Success 0.0699ms (my) vs 0.3611ms (O(n^2))

Matching 'fbdcdcdgg' resulted in:

                 Success 0.0699ms (my) vs 0.3011ms (O(n^2))

Matching 'fbdacgg' resulted in:

                 Success 0.0412ms (my) vs 0.3158ms (O(n^2))

Matching 'fcdabgg' resulted in:

                 Success 0.0568ms (my) vs 0.2882ms (O(n^2))

Matching 'fcdababgg' resulted in:

                 Success 0.0648ms (my) vs 0.2941ms (O(n^2))

Matching 'fcdabcdgg' resulted in:

                 Success 0.0648ms (my) vs 0.294ms (O(n^2))

Matching 'fcdcdcdgg' resulted in:

                 Success 0.1099ms (my) vs 0.4177ms (O(n^2))

Matching 'fcdacgg' resulted in:

                 Success 0.0339ms (my) vs 0.286ms (O(n^2))

Matching 'fccdabgg' resulted in:

                 Success 0.0231ms (my) vs 0.2723ms (O(n^2))

Matching 'fccdababgg' resulted in:

                 Success 0.0212ms (my) vs 0.2764ms (O(n^2))

Matching 'fccdabcdgg' resulted in:

                 Success 0.0204ms (my) vs 0.2769ms (O(n^2))

Matching 'fccdcdcdgg' resulted in:

                 Success 0.029ms (my) vs 0.3569ms (O(n^2))

Matching 'fccdacgg' resulted in:

                 Success 0.0196ms (my) vs 0.2777ms (O(n^2))

Matching 'dab' resulted in:

                 Success 0.0353ms (my) vs 0.3292ms (O(n^2))

Matching 'dabcabc' resulted in:

                 Success 0.0634ms (my) vs 0.3239ms (O(n^2))

Matching 'dabc' resulted in:

                 Success 0.0521ms (my) vs 0.4646ms (O(n^2))

Matching 'ddefdef' resulted in:

                 Success 0.0587ms (my) vs 0.4771ms (O(n^2))

Matching 'dabcg' resulted in:

                 Success 0.0552ms (my) vs 0.303ms (O(n^2))

Matching 'dg' resulted in:

                 Success 0.0358ms (my) vs 0.3173ms (O(n^2))

Matching 'dabcabcg' resulted in:

                 Success 0.0746ms (my) vs 0.3111ms (O(n^2))

Matching 'ddefabcg' resulted in:

                 Success 0.0658ms (my) vs 0.303ms (O(n^2))

Matching 'ddefdefg' resulted in:

                 Success 0.0642ms (my) vs 0.2991ms (O(n^2))

Total 3.7746ms (my) vs 15.5434ms (O(n^2))