#ifndef __GRAPHCHI_RMSE_ENGINE4
#define __GRAPHCHI_RMSE_ENGINE4
/**
 * @file
 * @author  Danny Bickson
 * @version 1.0
 *
 * @section LICENSE
 *
 * Copyright [2012] [Carnegie Mellon University]
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 * http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * File for aggregating and siplaying error mesasures and algorithm progress
 */

float (*pprediction_func)(const vertex_data&, const vertex_data&, const float, double &, void *) = NULL;
vec validation_rmse_vec;
bool user_nodes = true;
int counter = 0;
bool time_weighting = false;
/**
 * GraphChi programs need to subclass GraphChiProgram<vertex-type, edge-type> 
 * class. The main logic is usually in the update function.
 */
struct ValidationRMSEProgram4 : public GraphChiProgram<VertexDataType, EdgeDataType> {

  /**
   *  compute validaton RMSE for a single user
   */
  void update(graphchi_vertex<VertexDataType, EdgeDataType> &vertex, graphchi_context &gcontext) {
    if (user_nodes && vertex.id() >= M)
      return;
    else if (!user_nodes && vertex.id() < M)
      return;
    vertex_data & vdata = latent_factors_inmem[vertex.id()];
    for(int e=0; e < vertex.num_outedges(); e++) {
      double observation = vertex.edge(e)->get_data().weight;                
      vertex_data & nbr_latent = latent_factors_inmem[vertex.edge(e)->vertex_id()];
      double prediction;
      double rmse = (*pprediction_func)(vdata, nbr_latent, observation, prediction, NULL);
      assert(rmse <= pow(maxval - minval, 2));
      if (time_weighting)
        rmse *= vertex.edge(e)->get_data().time;
      validation_rmse_vec[omp_get_thread_num()] += rmse;
    }
  }

  void before_iteration(int iteration, graphchi_context & gcontext){
    last_validation_rmse = dvalidation_rmse;
    validation_rmse_vec = zeros(number_of_omp_threads());
  }
  /**
   * Called after an iteration has finished.
   */
  void after_iteration(int iteration, graphchi_context &gcontext) {
    assert(Le > 0);
    dvalidation_rmse = finalize_rmse(sum(validation_rmse_vec) , (double)Le);
    std::cout<<"  Validation  " << error_names[loss_type] << ":" << std::setw(10) << dvalidation_rmse << std::endl;
  if (halt_on_rmse_increase && dvalidation_rmse > last_validation_rmse && gcontext.iteration > 0){
    logstream(LOG_WARNING)<<"Stopping engine because of validation RMSE increase" << std::endl;
    gcontext.set_last_iteration(gcontext.iteration);
    }
  }
};


template<typename VertexDataType, typename EdgeDataType>
void init_validation_rmse_engine(graphchi_engine<VertexDataType,EdgeDataType> *& pvalidation_engine, int nshards,float (*prediction_func)(const vertex_data & user, const vertex_data & movie, float rating, double & prediction, void * extra), bool _time_weighting){
  metrics * m = new metrics("validation_rmse_engine");
  graphchi_engine<VertexDataType, EdgeDataType> * engine = new graphchi_engine<VertexDataType, EdgeDataType>(validation, nshards, false, *m); 
  set_engine_flags(*engine);
  pvalidation_engine = engine;
  time_weighting = _time_weighting;
  pprediction_func = prediction_func;
}


template<typename VertexDataType, typename EdgeDataType>
void run_validation4(graphchi_engine<VertexDataType, EdgeDataType> * pvalidation_engine, graphchi_context & context){
   //no validation data, no need to run validation engine calculations
   if (pvalidation_engine == NULL)
     return;
   ValidationRMSEProgram4 program;
   pvalidation_engine->run(program, 1);
}
#endif //__GRAPHCHI_RMSE_ENGINE4
