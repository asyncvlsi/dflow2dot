#-------------------------------------------------------------------------
#
#  Copyright (c) 2022 Rajit Manohar
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#  
#-------------------------------------------------------------------------
LIB1=dflowgraph_pass_$(EXT).so
LIB2=actgraph_pass_$(EXT).so

TARGETLIBS=$(LIB1) $(LIB2)
TARGETSCRIPTS=dflow2dot act2dot

SHOBJS1=dflowgraph.os
SHOBJS2=actgraph.os

SHOBJS=$(SHOBJS1) $(SHOBJS2)

SRCS=$(SHOBJS:.os=.cc)

include $(ACT_HOME)/scripts/Makefile.std

$(LIB1): $(SHOBJS1)
	$(ACT_HOME)/scripts/linkso $(LIB1) $(SHOBJS1) $(SHLIBACTPASS)

$(LIB2): $(SHOBJS2)
	$(ACT_HOME)/scripts/linkso $(LIB2) $(SHOBJS2) $(SHLIBACTPASS)



CXXFLAGS=-std=c++17
CPPSTD=c++17
-include Makefile.deps
