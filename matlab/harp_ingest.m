function varargout = harp_ingest(varargin)
% HARP_INGEST  Ingest data from a product file.
%
%   PRODUCT = HARP_INGEST(FILEPATH) reads data from the product file(s)
%   specified by filepath.
%
%   PRODUCT = HARP_INGEST(FILEPATH, FILTER) reads data matching the
%   filter from the product file(s) specified by filepath.
%
%   The filepath parameter must, in case you want to ingest a single
%   product file, be a string containing the full path (or relative path
%   with respect to the current working directory) of the product file.
%
%   If you want to ingest multiple files at one, just provide a two
%   dimensional character array or a one dimensional cell array with
%   the full paths to the product files. All product files should be
%   of the same product type for the ingestion to succeed.
%
%   The filter should be a single string containing a ',' or ';'
%   separated list of filter options.
%
%   More information about HARP products can be found in the HARP Data
%   Description documentation.
%

% Call HARP_MATLAB.MEX to do the actual work.
[varargout{1:max(1,nargout)}] = harp_matlab('INGEST',varargin{:});
