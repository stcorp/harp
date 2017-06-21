function varargout = harp_export(varargin)
% HARP_EXPORT  Export a HARP product to a file.
%
%   HARP_EXPORT(PRODUCT, FILEPATH, FORMAT) will export the given product
%   to the file that is specified by filepath. The export formats that
%   are supported are:
%     - hdf4
%     - hdf5
%     - netcdf
%   You should provide the format as a string to HARP_EXPORT. If the
%   export file already exists it will be overwritten.
%
%   See also HARP_IMPORT
%

% Call HARP_MATLAB.MEX to do the actual work.
harp_matlab('EXPORT',varargin{:});
