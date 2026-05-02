import React from "react";
import IconButton from "@mui/material/IconButton";
import CancelIcon from "@mui/icons-material/CancelOutlined";
import SearchIcon from "@mui/icons-material/Search";
import FormControl from "@mui/material/FormControl";
import OutlinedInput from '@mui/material/OutlinedInput';
import InputAdornment from '@mui/material/InputAdornment';

type Props = {
  value: string;
  onChange: (next: string) => void;
};

export default function Search({ value, onChange }: Props) {
  return(
    <FormControl fullWidth sx={{ m: 1 }}>
      <OutlinedInput
        id="outlined-adornment-search"
        placeholder="Search"
        value={value}
        onChange={(e) => onChange(e.target.value)}
        startAdornment={
          <InputAdornment position="start">
            <SearchIcon sx={{color: 'primary.main'}} />
          </InputAdornment>
        }
        endAdornment={
          value && (
            <InputAdornment position="end">
              <IconButton aria-label="clear search" onClick={() => onChange('')} edge="end" size="small" sx={{width: 36, height: 36}}>
              <CancelIcon sx={{width: 36, height: 36, color: 'secondary.light'}} />
                </IconButton>
            </InputAdornment>
          )
        }
      />
    </FormControl>
  );
}