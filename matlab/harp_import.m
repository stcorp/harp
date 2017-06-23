function varargout = harp_import(varargin)
% HARP_IMPORT  Import data from a product file.
%
%   PRODUCT = HARP_IMPORT(FILEPATH) reads data from the product file(s)
%   specified by filepath.
%
%   PRODUCT = HARP_IMPORT(FILEPATH, OPERATIONS='') reads data from the
%   product file(s) specified by filepath and performs operations on that data.
%
%   PRODUCT = HARP_IMPORT(FILEPATH, OPERATIONS='', OPTIONS='') reads data
%   matching the options from the product file(s) specified by filepath
%   and performs operations on that data. 
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
%   OPERATIONS - Actions to apply as part of the import; should be
%   specified as a semi-colon separated string of operations.
%
%   OPTIONS – Ingestion module specific options; should be specified
%   as a semi-colon separated string of key=value pairs; only used
%   if the file is not in HARP format.
%
%   More information about HARP products can be found in the HARP Data
%   Description documentation.
%
%   See also HARP_EXPORT
%

% Call HARP_MATLAB.MEX to do the actual work.
[varargout{1:max(1,nargout)}] = harp_matlab('IMPORT',varargin{:});
