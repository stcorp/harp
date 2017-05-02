function varargout = harp_import(varargin)
% HARP_IMPORT  Import data from a product file.
%
%   PRODUCT = HARP_IMPORT(FILEPATH) reads data from the product file(s)
%   specified by filepath.
%
%   PRODUCT = HARP_IMPORT(FILEPATH, FILTER) reads data matching the
%   filter from the product file(s) specified by filepath.
%
%   The filepath parameter must, in case you want to import a single
%   product file, be a string containing the full path (or relative path
%   with respect to the current working directory) of the product file.
%
%   If you want to import multiple files at one, just provide a two
%   dimensional character array or a one dimensional cell array with
%   the full paths to the product files. All product files should be
%   of the same product type for the import to succeed.
%
%   The filter should be a single string containing a ',' or ';'
%   separated list of filter options.
%
%   More information about HARP products can be found in the HARP Data
%   Description documentation.
%

% Call HARP_MATLAB.MEX to do the actual work.
[varargout{1:max(1,nargout)}] = harp_matlab('IMPORT',varargin{:});
