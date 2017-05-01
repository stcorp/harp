function varargout = harp_version(varargin)
% HARP_VERSION  Get version number of HARP.
%
%   VERSION = HARP_VERSION returns the version number of HARP.
%

% Call HARP_MATLAB.MEX to do the actual work.
[varargout{1:max(1,nargout)}] = harp_matlab('VERSION',varargin{:});
