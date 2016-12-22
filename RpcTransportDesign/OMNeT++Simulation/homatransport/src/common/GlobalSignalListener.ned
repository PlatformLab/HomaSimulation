//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
//

package homatransport.common;

//
// This module is intended as place at the top simulation level that collects
// signals and record statistics over the whole simulation. For example, you
// can define a new listener in this module that receives signals from
// all transport modules and aggregate some statistic over those signals and 
// and record the statistics in vector result files.
//

simple GlobalSignalListener 
{
    parameters:
        @display("i=status/lightning");
}
