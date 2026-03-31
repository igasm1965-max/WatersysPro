import { createContext, useContext } from 'react';

interface AppContextType {
  lockApp: () => void;
}

export const AppContext = createContext<AppContextType>({ lockApp: () => {} });

export const useAppContext = () => useContext(AppContext);
