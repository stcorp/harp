function varargout = harp_import(varargin)
% HARP_IMPORT  Import a HARP product from a file.
%
%   PRODUCT = HARP_IMPORT(FORMAT, FILEPATH) will import the record
%   stored in the file that is specified by filepath. You should specify
%   the format of the file with the format parameter. Possible values
%   for format are:
%     - ASCII
%     - BINARY
%     - HDF4
%     - HDF5
% 	  - NETCDF
%   You should provide the format as a string to HARP_IMPORT.
%
%   See also HARP_EXPORT
%

% Call HARP_MATLAB.MEX to do the actual work.
[varargout{1:max(1,nargout)}] = harp_matlab('IMPORT',varargin{:});
