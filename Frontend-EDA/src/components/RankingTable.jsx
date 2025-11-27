import React from 'react';

/**
 * Componente de tabla de rankings para índices métricos.
 * Muestra rankings de PA, Compdists y Running Time para MRQ y MkNN.
 */
export default function RankingTable({ rankings, title }) {
  if (!rankings || (!rankings.MRQ && !rankings.MkNN)) {
    return (
      <div className="bg-[#141416] border border-white/10 rounded-xl p-6">
        <h2 className="text-xl font-bold mb-4 text-gray-100">{title}</h2>
        <p className="text-gray-400">No hay datos de ranking disponibles</p>
      </div>
    );
  }

  const formatValue = (value, metric) => {
    if (value === undefined || value === null) return '-';
    
    if (metric === 'time_ms') {
      return value.toFixed(2) + ' ms';
    } else if (metric === 'pages') {
      return Math.round(value).toLocaleString();
    } else if (metric === 'compdists') {
      return Math.round(value).toLocaleString();
    }
    return value.toLocaleString();
  };

  const getRankClass = (rank) => {
    if (rank === 1) return 'text-yellow-400 font-bold';
    if (rank === 2) return 'text-gray-300 font-semibold';
    if (rank === 3) return 'text-orange-400 font-semibold';
    return 'text-gray-400';
  };

  const renderQuerySection = (queryType) => {
    const queryData = rankings[queryType];
    if (!queryData) return null;

    const metrics = [
      { key: 'pages', label: 'PA (Pages)' },
      { key: 'compdists', label: 'Compdists' },
      { key: 'time_ms', label: 'Running Time' }
    ];

    return (
      <div key={queryType} className="space-y-4">
        <h3 className="text-lg font-semibold text-sky-400 border-b border-white/10 pb-2">
          {queryType}
        </h3>
        
        <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
          {metrics.map(({ key, label }) => {
            const data = queryData[key] || [];
            
            return (
              <div key={key} className="bg-[#1a1a1c] rounded-lg p-4 border border-white/5">
                <h4 className="text-sm font-medium text-gray-300 mb-3 text-center">
                  {label}
                </h4>
                
                <div className="space-y-2">
                  {data.length === 0 ? (
                    <p className="text-gray-500 text-sm text-center">Sin datos</p>
                  ) : (
                    data.slice(0, 10).map((item) => (
                      <div
                        key={item.index}
                        className="flex items-center justify-between p-2 rounded bg-[#0f0f10] hover:bg-[#252527] transition-colors"
                      >
                        <div className="flex items-center gap-3 flex-1">
                          <span className={`text-sm font-mono w-6 text-center ${getRankClass(item.rank)}`}>
                            #{item.rank}
                          </span>
                          <span className="text-sm font-medium text-gray-200">
                            {item.index}
                          </span>
                        </div>
                        <span className="text-xs text-gray-400 font-mono">
                          {formatValue(item.value, key)}
                        </span>
                      </div>
                    ))
                  )}
                </div>
              </div>
            );
          })}
        </div>
      </div>
    );
  };

  return (
    <div className="bg-[#141416] border border-white/10 rounded-xl p-6 shadow-lg">
      <h2 className="text-2xl font-bold mb-6 text-gray-100 border-b border-white/10 pb-3">
        {title}
      </h2>
      
      <div className="space-y-8">
        {renderQuerySection('MRQ')}
        {renderQuerySection('MkNN')}
      </div>
    </div>
  );
}
