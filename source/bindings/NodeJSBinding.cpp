/**
 
Copyright (c) 2014 National Instruments Corp.
 
This software is subject to the terms described in the LICENSE.TXT file
 
SDG
*/

/*! \file
    \brief Bindings for making a Node.JS native module
 */
 
#include <node.h>
#include <v8.h>

using namespace v8;

Handle<Value> Method(const Arguments& args) {
  HandleScope scope;
  return scope.Close(String::New("chirp chirp"));
}

void init(Handle<Object> target) {
  target->Set(String::NewSymbol("vireo"),
      FunctionTemplate::New(Method)->GetFunction());
}

NODE_MODULE(vireo, init)